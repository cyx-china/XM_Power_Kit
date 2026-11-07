/**
 * @file    DpsGraph.c
 * @brief   DPS 曲线显示页：10 秒电压/电流/功率趋势图
 *
 * 渲染思路：
 *   绘图区（x:52..317, y:110..239）按 8 列一条切成 34 块，每块把 130×8 的像素
 *   直接在静态缓冲里合成好（底色 → 小图底槽 → 面积填充 → 折线 → 参考虚线 → 刻度），
 *   然后一次 lcd_draw_data() 交给刷屏线程做 DMA，一帧只需 34 次命令。
 *   刻度标签（左栏 x:0..51）单独绘制，文字不变就不重画。
 */

#include <math.h>
#include <string.h>

#include "DpsGraph.h"
#include "graphics.h"
#include "lcd_draw_api.h"
#include "font.h"
#include "format.h"

/* ========================== 布局常量 ========================== */
#define DPS_CURVE_NUM       3

#define CHART_TOP           110
#define CHART_BOTTOM        239

#define PLOT_X0             52
#define PLOT_X1             317
#define PLOT_W              (PLOT_X1 - PLOT_X0 + 1)         /* 266 列 */

#define LABEL_RIGHT         50                              /* 刻度文字右对齐到此处 */

#define PANE_TOP            111
#define PANE_H              40
#define PANE_GAP            3
#define PANE_Y(i)           (PANE_TOP + (i) * (PANE_H + PANE_GAP))
#define PANE_BOTTOM(i)      (PANE_Y(i) + PANE_H - 1)

#define BLOCK_W             8                               /* 一行 8 列：块缓冲 8*130*2=2080B，一帧 34 次 DMA */
#define BLOCK_ROWS          (CHART_BOTTOM - CHART_TOP + 1)  /* 130 行 */
#define GRID_DIV            5                               /* 横向 5 等分，每格 2 秒 */

/* 一列一点：采样点数必须等于绘图区列数，否则曲线滚动时要重采样，历史形状会跟着变 */
#if (DPS_GRAPH_SAMPLE_NUM) != (PLOT_X1 - PLOT_X0 + 1)
#error "DPS_GRAPH_SAMPLE_NUM 必须等于绘图区列数 PLOT_W"
#endif

/* ========================== 颜色 ========================== */
#define COL_BG              0x1082      /* 绘图区底色 */
#define COL_PLOT_BG         0x18E3      /* 小图底槽 */
#define COL_GRID            0x2945      /* 网格/边框 */
#define COL_TARGET          0x8410      /* 目标值参考线（中灰，压在面积填充上也看得见） */

/* ========================== 类型定义 ========================== */
typedef struct {
    float voltage;
    float current;
    float power;
} DpsSample_t;

typedef struct {
    uint16_t color;         /* 曲线颜色 */
    const char *unit;       /* 基准单位 */
    const char *unit_milli; /* 小单位（<1 时使用） */
    float min_span;         /* 自动量程的最小跨度 */
} CurveDesc_t;

typedef struct {
    float lo;               /* 量程下限 */
    float hi;               /* 量程上限 */
    float scale;            /* 行/单位 */
    uint8_t valid;          /* 是否已建立量程 */
    char lbl_hi[8];         /* 上限刻度文字 */
    char lbl_lo[8];         /* 下限刻度文字 */
} Axis_t;

/* ========================== 静态数据 ========================== */
static const CurveDesc_t curve_desc[DPS_CURVE_NUM] = {
    {0xEB0C, "V", "mV", 0.2f},      /* 电压：主界面同色 */
    {0x5CBD, "A", "mA", 0.1f},      /* 电流 */
    {0xDB3F, "W", "mW", 0.2f},      /* 功率 */
};

static DpsSample_t samples[DPS_GRAPH_SAMPLE_NUM];
static uint16_t sample_head = 0;    /* 下一个写入位置 */
static uint16_t sample_count = 0;   /* 有效点数（最大 DPS_GRAPH_SAMPLE_NUM） */

static Axis_t axis[DPS_CURVE_NUM];
static char shown_lbl_hi[DPS_CURVE_NUM][8];
static char shown_lbl_lo[DPS_CURVE_NUM][8];
static uint8_t labels_drawn = 0;

/* 一条带（8 列 × 130 行）的像素缓冲：2080 字节 */
static uint16_t block_buf[BLOCK_W * BLOCK_ROWS];

/* ========================== 采样缓冲 ========================== */
void DpsGraph_Reset(void) {
    memset(samples, 0, sizeof(samples));
    sample_head = 0;
    sample_count = 0;

    for (uint8_t i = 0; i < DPS_CURVE_NUM; i++) {
        axis[i].valid = 0;
        axis[i].lo = 0.0f;
        axis[i].hi = 0.0f;
        axis[i].scale = 1.0f;
        axis[i].lbl_hi[0] = '\0';
        axis[i].lbl_lo[0] = '\0';
    }
    labels_drawn = 0;
}

void DpsGraph_Sample(float voltage, float current, float power) {
    samples[sample_head].voltage = voltage;
    samples[sample_head].current = current;
    samples[sample_head].power = power;

    sample_head++;
    if (sample_head >= DPS_GRAPH_SAMPLE_NUM) sample_head = 0;
    if (sample_count < DPS_GRAPH_SAMPLE_NUM) sample_count++;
}

/* index: 0 = 最旧的样本 */
static const DpsSample_t *sample_at(uint16_t index) {
    uint16_t pos = sample_head + DPS_GRAPH_SAMPLE_NUM - sample_count + index;
    if (pos >= DPS_GRAPH_SAMPLE_NUM) pos -= DPS_GRAPH_SAMPLE_NUM;
    if (pos >= DPS_GRAPH_SAMPLE_NUM) pos -= DPS_GRAPH_SAMPLE_NUM;
    return &samples[pos];
}

static float sample_value(const DpsSample_t *s, uint8_t ci) {
    if (ci == 0) return s->voltage;
    if (ci == 1) return s->current;
    return s->power;
}

/* ========================== 自动量程 ========================== */
/**
 * @brief  把跨度取整到 1/2/5 × 10^n
 * @note   不用 log10f/powf，避免引入多余的 libm 依赖
 */
static float nice_step(float raw) {
    if (!(raw > 0.0f)) return 1.0f;

    float scale = 1.0f;
    while (raw >= 10.0f) {
        raw *= 0.1f;
        scale *= 10.0f;
    }
    while (raw < 1.0f) {
        raw *= 10.0f;
        scale *= 0.1f;
    }

    float m = (raw <= 1.0f) ? 1.0f : (raw <= 2.0f) ? 2.0f : (raw <= 5.0f) ? 5.0f : 10.0f;
    return m * scale;
}

/**
 * @brief  生成上限/下限刻度文字（单位随量级自适应）
 */
static void axis_make_labels(uint8_t ci) {
    Axis_t *ax = &axis[ci];
    const CurveDesc_t *desc = &curve_desc[ci];

    /* 上限小于 1 时改用小单位（mV/mA/mW），保证 3 位有效数字 */
    uint8_t use_milli = (ax->hi < 1.0f) ? 1 : 0;
    float factor = use_milli ? 1000.0f : 1.0f;
    const char *suffix = use_milli ? desc->unit_milli : desc->unit;

    float hi_s = ax->hi * factor;
    uint8_t dec = (hi_s >= 100.0f) ? 0 : (hi_s >= 10.0f) ? 1 : 2;

    fmt_fixed_unit(ax->lbl_hi, sizeof(ax->lbl_hi), hi_s, dec, 0, 0, suffix);
    fmt_fixed_unit(ax->lbl_lo, sizeof(ax->lbl_lo), ax->lo * factor, dec, 0, 0, suffix);
}

/**
 * @brief  根据采样窗口更新一个通道的量程
 * @param  ci          通道索引
 * @param  ref         参考值（电源开启时为设定值，会被并入量程）
 * @param  use_ref     是否使用参考值
 * @note   带迟滞：只有越界或原量程明显过宽时才换挡，避免刻度来回跳
 */
static void axis_update(uint8_t ci, float ref, uint8_t use_ref) {
    float mn = 0.0f, mx = 0.0f;
    uint8_t first = 1;

    for (uint16_t i = 0; i < sample_count; i++) {
        float v = sample_value(sample_at(i), ci);
        if (first) {
            mn = v;
            mx = v;
            first = 0;
        } else {
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
    }

    if (use_ref) {
        if (first) {
            mn = ref;
            mx = ref;
            first = 0;
        } else {
            if (ref < mn) mn = ref;
            if (ref > mx) mx = ref;
        }
    }

    if (first) {
        mn = 0.0f;
        mx = 0.0f;
    }

    /* 跨度太小时以中点撑开（避免平坦直线被放大成噪声） */
    float span = mx - mn;
    if (span < curve_desc[ci].min_span) {
        float mid = 0.5f * (mn + mx);
        mn = mid - curve_desc[ci].min_span * 0.5f;
        mx = mid + curve_desc[ci].min_span * 0.5f;
        span = curve_desc[ci].min_span;
    }

    /* 上下各留 6% 余量，避免曲线贴边 */
    mn -= span * 0.06f;
    mx += span * 0.06f;

    /* 电压/电流/功率均非负：下限先钳到 0，否则余量把 mn 压成负数后会一直
       满足"mn < ax->lo"，导致每帧都重算量程（看着没变但白白跑一遍） */
    if (mn < 0.0f) mn = 0.0f;

    /* 取整到 1/2/5 步长 */
    float step = nice_step((mx - mn) / 3.0f);
    float lo = floorf(mn / step) * step;
    float hi = ceilf(mx / step) * step;
    if (lo < 0.0f) lo = 0.0f;       /* 电压/电流/功率均非负 */
    if (hi <= lo) hi = lo + step;

    Axis_t *ax = &axis[ci];
    uint8_t apply = 0;

    if (!ax->valid) {
        apply = 1;
    } else if (mx > ax->hi || mn < ax->lo) {
        apply = 1;                                  /* 曲线要超出量程：立刻放宽 */
    } else {
        float cur_span = ax->hi - ax->lo;
        /* 新量程明显更窄（不足当前 60%）时收紧一档。
           不能要求"上下两端各缩 35%"：电压/电流/功率都非负，下限常被钳到 0，
           下限永远缩不动，量程就只会放大不会回落（15V 调到 1V 后卡在 0~15V） */
        if (((hi - lo) < cur_span * 0.6f) && (hi <= ax->hi) && (lo >= ax->lo)) apply = 1;
    }

    if (!apply) return;

    ax->lo = lo;
    ax->hi = hi;
    ax->scale = (float)(PANE_H - 1) / (hi - lo);
    ax->valid = 1;
    axis_make_labels(ci);
}

/* ========================== 坐标换算 ========================== */
/* 屏幕列 -> 采样索引：一列一点（最新点在最右列），返回负数表示该列还没有数据。
   整数映射是关键：新点进缓冲时整条曲线只向左平移 1 像素，形状不变；
   若在列与点之间做小数插值，每帧相位不同，历史曲线就会一直"蠕动"。 */
static int16_t column_index(uint16_t col) {
    if (sample_count == 0) return -1;

    int16_t idx = (int16_t)((int16_t)(sample_count - 1) - (int16_t)(PLOT_W - 1 - col));
    if (idx < 0) return -1;
    if (idx >= (int16_t)sample_count) idx = (int16_t)(sample_count - 1);
    return idx;
}

static int16_t value_to_y(uint8_t ci, float v) {
    float y = (float)PANE_BOTTOM(ci) - (v - axis[ci].lo) * axis[ci].scale + 0.5f;
    if (y < (float)PANE_Y(ci)) y = (float)PANE_Y(ci);
    if (y > (float)PANE_BOTTOM(ci)) y = (float)PANE_BOTTOM(ci);
    return (int16_t)y;
}

/* ========================== 刻度文字 ========================== */
/* 与 graphics.c 的字宽步进保持一致：小数点只占 font_width*6/10+1，其余占 font_width */
static uint16_t text_width(const FontInfo *font, const char *s) {
    uint16_t w = 0;
    uint16_t dot_w = (uint16_t)(font->font_width * 6 / 10 + 1);

    while (*s) {
        if (*s == '.') w = (uint16_t)(w + dot_w);
        else w = (uint16_t)(w + font->font_width);
        s++;
    }
    return w;
}

static void draw_label(uint16_t y, const char *text, uint16_t color) {
    uint16_t w = text_width(&NotoMono10x14, text);
    uint16_t x = (w < LABEL_RIGHT) ? (uint16_t)(LABEL_RIGHT - w) : 0;

    lcd_draw_rect(0, y, LABEL_RIGHT + 1, (uint16_t)(y + NotoMono10x14.font_height - 1), COL_BG, 1);
    lcd_draw_string(x, y, text, &NotoMono10x14, color, COL_BG, 0);
}

static void labels_refresh(void) {
    for (uint8_t ci = 0; ci < DPS_CURVE_NUM; ci++) {
        if (!labels_drawn || strcmp(shown_lbl_hi[ci], axis[ci].lbl_hi) != 0) {
            strcpy(shown_lbl_hi[ci], axis[ci].lbl_hi);
            draw_label((uint16_t)(PANE_Y(ci) + 1), axis[ci].lbl_hi, curve_desc[ci].color);
        }
        if (!labels_drawn || strcmp(shown_lbl_lo[ci], axis[ci].lbl_lo) != 0) {
            strcpy(shown_lbl_lo[ci], axis[ci].lbl_lo);
            draw_label((uint16_t)(PANE_BOTTOM(ci) - 13), axis[ci].lbl_lo, curve_desc[ci].color);
        }
    }
    labels_drawn = 1;
}

/* ========================== 当前值标签 ========================== */
/**
 * @brief  在小图里标出最新采样值
 * @note   横向固定在右侧（宽度只随数字位数变，不随曲线漂移），
 *         纵向跟随最新点的 y：默认贴在曲线末端上方，上方越界时改画到曲线下方。
 *         放在 render_curves() 之后调用，块 DMA 已阻塞完成，文字一定压在本帧像素之上。
 */
static void draw_value_tag(uint8_t ci) {
    if (sample_count == 0 || !axis[ci].valid) return;

    const CurveDesc_t *desc = &curve_desc[ci];
    float v = sample_value(sample_at((uint16_t)(sample_count - 1)), ci);

    /* 单位与纵轴刻度保持一致：量程上限小于 1 时用 mV/mA/mW */
    uint8_t use_milli = (axis[ci].hi < 1.0f) ? 1 : 0;
    char text[12];
    int len = fmt_fixed_digits(v * (use_milli ? 1000.0f : 1.0f), 4, text);

    const char *unit = use_milli ? desc->unit_milli : desc->unit;
    while (*unit && len < (int)sizeof(text) - 1) text[len++] = *unit++;
    text[len] = '\0';

    /* 当前值标签用 5x7 小字模，只有刻度文字的一半大小，尽量少盖住曲线 */
    const FontInfo *tag_font = &Mono5x7;

    uint16_t w = text_width(tag_font, text);
    uint16_t x = (uint16_t)(PLOT_X1 - 2 - w);
    if (x < PLOT_X0) x = PLOT_X0;

    uint16_t fh = tag_font->font_height;

    int16_t y_now = value_to_y(ci, v);
    int16_t y_hi = y_now;       /* 标签会盖住右侧这段曲线：取段内最高点（y 最小） */
    int16_t y_lo = y_now;       /* 段内最低点（y 最大） */

    for (uint16_t col = (uint16_t)(x - PLOT_X0); col < PLOT_W; col++) {
        int16_t idx = column_index(col);
        if (idx < 0) continue;

        int16_t yc = value_to_y(ci, sample_value(sample_at((uint16_t)idx), ci));
        if (yc < y_hi) y_hi = yc;
        if (yc > y_lo) y_lo = yc;
    }

    int16_t y = (int16_t)(y_hi - (int16_t)fh - 2);
    uint16_t bg = COL_PLOT_BG;

    if (y < (int16_t)(PANE_Y(ci) + 1)) {
        /* 上方顶出小图：改画到曲线下方，用面积填充色做底减少突兀感 */
        y = (int16_t)(y_lo + 3);
        bg = (uint16_t)((desc->color >> 2) & 0x39E7);

        if (y > (int16_t)(PANE_BOTTOM(ci) - (int16_t)fh))
            y = (int16_t)(PANE_BOTTOM(ci) - (int16_t)fh);
        if (y < (int16_t)PANE_Y(ci)) y = (int16_t)PANE_Y(ci);
    }

    lcd_draw_string(x- 6, (uint16_t)y, text, tag_font, desc->color, bg, 1);
}

/* ========================== 整帧渲染 ========================== */
static void render_curves(float tgt_v, float tgt_c, uint8_t power_on) {
    int16_t prev_y[DPS_CURVE_NUM];

    for (uint8_t ci = 0; ci < DPS_CURVE_NUM; ci++) prev_y[ci] = -1;

    for (uint16_t bx = 0; bx < PLOT_W; bx += BLOCK_W) {
        uint16_t w = (uint16_t)((PLOT_W - bx < BLOCK_W) ? (PLOT_W - bx) : BLOCK_W);

        /* ---- 1. 整块先铺底色 ---- */
        for (uint16_t y = 0; y < BLOCK_ROWS; y++) {
            uint16_t *dst = &block_buf[y * w];
            for (uint16_t x = 0; x < w; x++) dst[x] = COL_BG;
        }

        /* ---- 2. 三个小图的底槽 + 上下边框 ---- */
        for (uint8_t ci = 0; ci < DPS_CURVE_NUM; ci++) {
            uint16_t ry0 = (uint16_t)(PANE_Y(ci) - CHART_TOP);
            uint16_t ry1 = (uint16_t)(PANE_BOTTOM(ci) - CHART_TOP);

            for (uint16_t y = ry0; y <= ry1; y++) {
                uint16_t c = (y == ry0 || y == ry1) ? COL_GRID : COL_PLOT_BG;
                uint16_t *dst = &block_buf[y * w];
                for (uint16_t x = 0; x < w; x++) dst[x] = c;
            }
        }

        /* ---- 3. 逐列合成三条曲线 ---- */
        for (uint16_t x = 0; x < w; x++) {
            uint16_t col = (uint16_t)(bx + x);
            int16_t idx = column_index(col);

            for (uint8_t ci = 0; ci < DPS_CURVE_NUM; ci++) {
                if (idx < 0) {
                    prev_y[ci] = -1;
                    continue;
                }

                uint16_t ryb = (uint16_t)(PANE_BOTTOM(ci) - CHART_TOP - 1);
                uint16_t fill = (uint16_t)((curve_desc[ci].color >> 2) & 0x39E7);
                int16_t y = value_to_y(ci, sample_value(sample_at((uint16_t)idx), ci));
                uint16_t ry = (uint16_t)(y - CHART_TOP);

                /* 面积填充：曲线下方填深色，让曲线更醒目 */
                for (uint16_t yy = ry; yy <= ryb; yy++) block_buf[yy * w + x] = fill;

                /* 折线：与左邻列连起来，避免陡坡出现断点 */
                int16_t y_from = (prev_y[ci] >= 0 && prev_y[ci] < y) ? prev_y[ci] : y;
                int16_t y_to = (prev_y[ci] > y) ? prev_y[ci] : y;
                if (y_from < (int16_t)PANE_Y(ci)) y_from = (int16_t)PANE_Y(ci);
                for (int16_t yy = y_from; yy <= y_to; yy++)
                    block_buf[(uint16_t)(yy - CHART_TOP) * w + x] = curve_desc[ci].color;

                prev_y[ci] = y;
            }

            /* ---- 4. 目标值参考虚线（电源开启时才画） ---- */
            if (power_on) {
                for (uint8_t ci = 0; ci < 2; ci++) {
                    float tv = (ci == 0) ? tgt_v : tgt_c;
                    if (tv < axis[ci].lo || tv > axis[ci].hi) continue;
                    if ((col & 3) >= 2) continue;           /* 2 点实、2 点空 */

                    int16_t y = value_to_y(ci, tv);
                    uint16_t ry = (uint16_t)(y - CHART_TOP);
                    if (ry > (uint16_t)(PANE_Y(ci) - CHART_TOP) &&
                        ry < (uint16_t)(PANE_BOTTOM(ci) - CHART_TOP))
                        block_buf[ry * w + x] = COL_TARGET;
                }
            }
        }

        /* ---- 5. 竖直刻度（小图上下的短刻度 + 小图间隙的分隔线） ---- */
        for (uint16_t x = 0; x < w; x++) {
            uint16_t col = (uint16_t)(bx + x);
            uint8_t is_grid = (col == 0 || col == PLOT_W - 1) ? 1 : 0;

            for (uint8_t g = 1; g < GRID_DIV && !is_grid; g++)
                if (col == (uint16_t)(PLOT_W * g / GRID_DIV)) is_grid = 1;

            if (!is_grid) continue;

            for (uint8_t ci = 0; ci < DPS_CURVE_NUM; ci++) {
                uint16_t ry0 = (uint16_t)(PANE_Y(ci) - CHART_TOP);
                uint16_t ry1 = (uint16_t)(PANE_BOTTOM(ci) - CHART_TOP);

                block_buf[(ry0 + 1) * w + x] = COL_GRID;
                block_buf[(ry0 + 2) * w + x] = COL_GRID;
                block_buf[(ry1 - 1) * w + x] = COL_GRID;
                block_buf[(ry1 - 2) * w + x] = COL_GRID;

                /* 小图之间的间隙画一条分隔刻度 */
                for (uint16_t yy = (uint16_t)(ry1 + 1);
                     ci + 1 < DPS_CURVE_NUM && yy < (uint16_t)(PANE_Y(ci + 1) - CHART_TOP); yy++)
                    block_buf[yy * w + x] = COL_GRID;
            }
        }

        /* ---- 6. 交给刷屏线程（阻塞到本块 DMA 完成，缓冲可安全复用） ---- */
        lcd_draw_data((uint16_t)(PLOT_X0 + bx), CHART_TOP,
                      (uint16_t)(PLOT_X0 + bx + w - 1), CHART_BOTTOM,
                      block_buf, (uint16_t)(w * BLOCK_ROWS));
    }
}

/* ========================== 对外接口 ========================== */
void DpsGraph_DrawChrome(void) {
    lcd_draw_rect(0, CHART_TOP, 319, CHART_BOTTOM, COL_BG, 1);
    labels_drawn = 0;       /* 让下一帧强制重画刻度 */
}

void DpsGraph_DrawFrame(float target_voltage, float target_current, uint8_t power_on) {
    axis_update(0, target_voltage, power_on);
    axis_update(1, target_current, power_on);
    axis_update(2, 0.0f, 0);

    labels_refresh();
    render_curves(target_voltage, target_current, power_on);

    /* 曲线画完再压最新值标签：render_curves 里每块 lcd_draw_data 都阻塞到 DMA 完成，
       所以这三段文字一定落在本帧所有像素之上，不会被块缓冲盖掉 */
    for (uint8_t ci = 0; ci < DPS_CURVE_NUM; ci++) draw_value_tag(ci);
}
