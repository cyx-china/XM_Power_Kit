#include "lv_async_utils.h"
#include <string.h>
#include "lvgl.h"

#define LV_ASYNC_COLOR_SKIP  0x0001u

/* ────────────────────────────── Label 专用回调 ────────────────────────────── */

static void async_update_label_cb(void *user_data)
{
    lv_async_label_update_t *p = (lv_async_label_update_t *)user_data;
    if (p == NULL || p->target == NULL) goto cleanup;

    // 更新文本（有内容才更新）
    if (p->text != NULL && p->text[0] != '\0')
    {
        if (lv_obj_has_class(p->target, &lv_label_class))
        {
            lv_label_set_text(p->target, p->text);
        }
    }

    // 更新颜色（非跳过值才设置）
    if (p->color != LV_ASYNC_COLOR_SKIP)
    {
        lv_color_t c;
        c.full = p->color;

        lv_obj_set_style_text_color(p->target, c, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    cleanup:
        if (p && p->text != NULL)
        {
            lv_mem_free((void *)p->text);
        }
    if (p)
    {
        lv_mem_free(p);
    }
}

/* ────────────────────────────── Label 对外接口 ────────────────────────────── */

void lv_async_set_label_text(lv_obj_t *target, const char *text, uint16_t color)
{
    if (target == NULL) return;

    lv_async_label_update_t *p = lv_mem_alloc(sizeof(lv_async_label_update_t));
    if (p == NULL) return;

    p->target = target;
    p->color  = color;
    p->text   = NULL;

    if (text != NULL && text[0] != '\0')
    {
        size_t len = strlen(text) + 1;
        char *buf = lv_mem_alloc(len);
        if (buf == NULL)
        {
            lv_mem_free(p);
            return;
        }
        memcpy(buf, text, len);
        p->text = buf;
    }

    lv_async_call(async_update_label_cb, p);
}