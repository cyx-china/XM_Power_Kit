/**
 ******************************************************************************
 * @file    boot_ota.c
 * @brief   开机决策：处理 REQ.BIN（升级/回滚）→ 确认上次升级结果 → 应用无效时兜底恢复。
 ******************************************************************************
 */
#include "boot.h"
#include "ff.h"
#include <string.h>

#define ROLLBACK_FAIL_LIMIT 3U

static bool read_req(boot_req_t *req)
{
    FIL file;
    UINT read = 0U;

    if (f_open(&file, BOOT_FILE_REQ, FA_READ) != FR_OK)
    {
        return false;
    }

    if (f_read(&file, req, sizeof(boot_req_t), &read) != FR_OK || read != sizeof(boot_req_t))
    {
        (void)f_close(&file);
        return false;
    }
    (void)f_close(&file);

    return req->crc == (uint16_t)(Boot_Crc32(req, 6U) & 0xFFFFU);
}

static bool read_bootok(boot_bootok_t *ok)
{
    FIL file;
    UINT read = 0U;

    if (f_open(&file, BOOT_FILE_BOOTOK, FA_READ) != FR_OK)
    {
        return false;
    }

    if (f_read(&file, ok, sizeof(boot_bootok_t), &read) != FR_OK || read != sizeof(boot_bootok_t))
    {
        (void)f_close(&file);
        return false;
    }
    (void)f_close(&file);

    return ok->magic == BOOT_BOOTOK_MAGIC &&
           ok->crc == Boot_Crc32(ok, 40U);
}

/* 进度：擦除 0~30%，写入 30~90%，回读校验 90~100% */
static void ui_progress(uint8_t stage, uint16_t percent)
{
    static uint8_t last_stage = 0xFFU;
    uint16_t overall;

    if (stage != last_stage)
    {
        last_stage = stage;
        if (stage == 0U)
        {
            Boot_Ui_Status(Boot_Txt_Erase, true);
        }
        else if (stage == 1U)
        {
            Boot_Ui_Status(Boot_Txt_Write, true);
        }
        else
        {
            Boot_Ui_Status(Boot_Txt_Check, true);
        }
    }

    if (stage == 0U)
    {
        overall = (uint16_t)((percent * 30U) / 100U);
    }
    else if (stage == 1U)
    {
        overall = (uint16_t)(30U + ((percent * 60U) / 100U));
    }
    else
    {
        overall = (uint16_t)(90U + ((percent * 10U) / 100U));
    }

    Boot_Ui_Progress(overall);
}

static bool program(const char *path, boot_record_t *rec, uint32_t token, bool require_confirm)
{
    boot_pkg_head_t head;

    Boot_Ui_Error("");                               /* 清掉上一次的错误行 */
    Boot_Ui_Status(Boot_Txt_Verify, true);
    Boot_Ui_Progress(0U);

    if (!Boot_Pkg_Load(path, &head))
    {
        Boot_Ui_Error(Boot_Txt_BadPkg);
        return false;
    }

    Boot_Ui_Package(head.version, head.image_size, head.image_crc);

    if (!Boot_Pkg_Flash(path, &head, ui_progress))
    {
        Boot_Ui_Error(Boot_Txt_WriteFail);
        return false;
    }

    /* 标记：新版已写入，等应用自己写 BOOTOK 确认 */
    memset(rec->version, 0, sizeof(rec->version));
    for (uint32_t i = 0U; i < (sizeof(rec->version) - 1U) && head.version[i] != '\0'; i++)
    {
        rec->version[i] = head.version[i];
    }
    rec->serial++;
    rec->token      = token;
    rec->fail_count = 0U;
    rec->state      = require_confirm ? BOOT_ST_WAIT_BOOTOK : BOOT_ST_IDLE;

    if (!Boot_State_Save(rec))
    {
        Boot_Ui_Error(Boot_Txt_WriteFail);
        return false;
    }

    (void)f_unlink(BOOT_FILE_REQ);
    (void)f_unlink(BOOT_FILE_BOOTOK);

    Boot_Ui_Status(Boot_Txt_Reboot, true);
    Boot_Ui_Progress(100U);

    return true;
}

void Boot_Ota_Boot(void)
{
    boot_record_t rec;
    boot_req_t    req;

    Boot_State_Load(&rec);

    /* 1) 应用写了 REQ.BIN：按请求搬运 */
    if (read_req(&req) && req.token != rec.token)
    {
        if (req.op == BOOT_OP_ROLLBACK)
        {
            if (program(BOOT_FILE_GOOD, &rec, req.token, false))
            {
                return;
            }
            Boot_Ui_Error(Boot_Txt_RollbackFail);
            return;
        }

        if (program(BOOT_FILE_FW, &rec, req.token, true))
        {
            return;
        }
        /* 新包刷不进去：退回上一个可用版本 */
        Boot_Ui_Error(Boot_Txt_WriteFail);
        if (program(BOOT_FILE_GOOD, &rec, req.token, false))
        {
            return;
        }
        Boot_Ui_Error(Boot_Txt_RollbackFail);
        return;
    }

    /* 2) 上一次升级还没确认启动成功 */
    if (rec.state == BOOT_ST_WAIT_BOOTOK)
    {
        boot_bootok_t ok;

        if (read_bootok(&ok) && (strncmp(ok.version, rec.version, sizeof(rec.version)) == 0))
        {
            rec.state      = BOOT_ST_IDLE;
            rec.fail_count = 0U;
            (void)Boot_State_Save(&rec);
            (void)f_unlink(BOOT_FILE_BOOTOK);
        }
        else
        {
            if (rec.fail_count < 0xFFU)
            {
                rec.fail_count++;
            }
            (void)Boot_State_Save(&rec);

            if (rec.fail_count >= ROLLBACK_FAIL_LIMIT)
            {
                Boot_Ui_Status(Boot_Txt_Rollback, false);
                if (program(BOOT_FILE_GOOD, &rec, rec.token, false))
                {
                    return;
                }
                Boot_Ui_Error(Boot_Txt_RollbackFail);
            }
        }
    }

    /* 3) 应用分区无效：用 GOOD.BIN 兜底恢复 */
    if (!Boot_App_Valid())
    {
        Boot_Ui_Status(Boot_Txt_Wait, false);
        if (program(BOOT_FILE_GOOD, &rec, rec.token, false))
        {
            return;
        }
    }
}
