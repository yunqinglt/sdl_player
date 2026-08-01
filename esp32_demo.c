#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
// #include "sdkconfig.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_chip_info.h"
// #include "esp_flash.h"
// #include "esp_timer.h"
// #include "esp_task_wdt.h"
// #include "lcd_init.h"
// #include "lcd.h"
// #include "pic.h"

// ============ 帧缓冲与查表 ============
static u8 rgb_buf[LCD_W * LCD_H * 2];     // 32KB RGB565 帧缓冲
static u16 palette[256];                  // 调色板
static u8 sintab[256];                    // 正弦查表
static u8 tunnel_dist[LCD_W * LCD_H];     // 隧道距离表
static u8 tunnel_ang[LCD_W * LCD_H];      // 隧道角度表

static inline u16 rgb565(u8 r, u8 g, u8 b)
{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// 初始化正弦表与调色板
static void init_tables(void)
{
    for (int i = 0; i < 256; i++) {
        float s = sinf(i * 6.283185f / 256.0f);
        sintab[i] = (u8)((s + 1.0f) * 127.5f);
    }
    // 彩虹调色板：黑→紫→蓝→青→绿→黄→红→白
    for (int i = 0; i < 256; i++) {
        u8 r, g, b;
        if      (i < 32)  { r = i*4; g = 0;       b = 128-i*4; }
        else if (i < 64)  { r = 128+(i-32)*4; g = 0; b = 0; }
        else if (i < 96)  { r = 255; g = (i-64)*4; b = 0; }
        else if (i < 128) { r = 255; g = 128+(i-96)*4; b = 0; }
        else if (i < 160) { r = 255-(i-128)*4; g = 255; b = (i-128)*4; }
        else if (i < 192) { r = 128-(i-160)*4; g = 255; b = 255; }
        else if (i < 224) { r = 0; g = 255-(i-192)*4; b = 255; }
        else              { r = (i-224)*8; g = (i-224)*8; b = 255; }
        palette[i] = rgb565(r, g, b);
    }
}

// 预计算隧道距离/角度表（只做一次）
static void init_tunnel(void)
{
    for (int y = 0; y < LCD_H; y++) {
        for (int x = 0; x < LCD_W; x++) {
            int dx = x - LCD_W / 2;
            int dy = y - LCD_H / 2;
            float d = sqrtf(dx*dx + dy*dy) * 3.5f;
            float a = atan2f(dy, dx);
            tunnel_dist[y*LCD_W + x] = (u8)((int)d & 0xFF);
            tunnel_ang[y*LCD_W + x] = (u8)((a + 3.14159f) * 40.7f);
        }
    }
}

// ============ 视频效果 0：经典 Plasma（等离子） ============
static void render_plasma(u8 t)
{
    u8 tt1 = t;
    u8 tt2 = t * 3;
    u8 tt3 = t * 5;
    for (int y = 0; y < LCD_H; y++) {
        int base = y * LCD_W * 2;
        u8 yv = (u8)y;
        for (int x = 0; x < LCD_W; x++) {
            u8 v = sintab[(x + tt1) & 0xFF]
                 + sintab[(yv + tt2) & 0xFF]
                 + sintab[(((x + yv) >> 1) + tt3) & 0xFF];
            u16 c = palette[v];
            rgb_buf[base + x*2]     = c >> 8;
            rgb_buf[base + x*2 + 1] = c;
        }
    }
}

// ============ 视频效果 1：3D 隧道（Tunnel） ============
static void render_tunnel(u8 t)
{
    u8 tt1 = t;
    u8 tt2 = t * 2;
    for (int i = 0; i < LCD_W * LCD_H; i++) {
        u8 v = sintab[(tunnel_dist[i] + tt1) & 0xFF]
             + sintab[(tunnel_ang[i] + tt2) & 0xFF];
        u16 c = palette[v];
        rgb_buf[i*2]     = c >> 8;
        rgb_buf[i*2 + 1] = c;
    }
}

// ============ 视频效果 2：同心圆 Moire（摩尔纹） ============
static void render_moire(u8 t)
{
    int cx = LCD_W / 2;
    int cy = LCD_H / 2;
    for (int y = 0; y < LCD_H; y++) {
        int base = y * LCD_W * 2;
        int dy = y - cy;
        int dy2 = dy * dy;
        for (int x = 0; x < LCD_W; x++) {
            int dx = x - cx;
            int d = (dx*dx + dy2) >> 4;
            u8 v = (d + (t << 3)) & 0xFF;
            u16 c = palette[v ^ (t * 11)];
            rgb_buf[base + x*2]     = c >> 8;
            rgb_buf[base + x*2 + 1] = c;
        }
    }
}

// ============ 视频效果 3：火焰（Fire） ============
static u8 fire_buf[LCD_W * LCD_H];

static void render_fire(u8 t)
{
    // 底部生成随机热源
    for (int x = 0; x < LCD_W; x++) {
        fire_buf[(LCD_H - 1) * LCD_W + x] = (rand() > RAND_MAX / 3) ? (rand() & 0xFF) : 0;
    }
    // 向上传播+冷却
    for (int y = 0; y < LCD_H - 2; y++) {
        for (int x = 1; x < LCD_W - 1; x++) {
            int idx = y * LCD_W + x;
            int below = (y + 1) * LCD_W + x;
            u8 v = (fire_buf[below - 1] + fire_buf[below] + fire_buf[below + 1] + fire_buf[below + LCD_W]) >> 2;
            fire_buf[idx] = (v > 1) ? (v - 1) : 0;
        }
    }
    // 查表转 RGB565（火焰专用调色板：黑→红→橙→黄→白）
    for (int i = 0; i < LCD_W * LCD_H; i++) {
        u8 v = fire_buf[i];
        u8 r, g, b;
        if (v < 32)       { r = v * 4; g = 0;       b = 0; }
        else if (v < 64)  { r = 128 + (v-32)*4; g = 0;       b = 0; }
        else if (v < 96)  { r = 255; g = (v-64)*4; b = 0; }
        else if (v < 128) { r = 255; g = 128+(v-96)*4; b = 0; }
        else              { r = 255; g = 255; b = (v-128)*2; }
        u16 c = rgb565(r, g, b);
        rgb_buf[i*2]     = c >> 8;
        rgb_buf[i*2 + 1] = c;
    }
}

// ============ FPS 显示 ============
static void draw_fps(u8 fps)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%02dFPS", fps);
    // 把 FPS 文字直接画到 rgb_buf 左上角（避免额外 SPI 事务）
    // 这里简化为只更新数字：用黑色方块覆盖 + 旧版字符串
    LCD_FillFast(0, 0, 48, 16, BLACK);
    LCD_ShowString(0, 0, (u8 *)buf, WHITE, BLACK, 16, 0);
}

// ============ 视频效果 4：图片弹跳 ============
static int px = 0, py = 0;
static int pvx = 4, pvy = 3;

static void anim_pic_bounce(void)
{
    int pw = 40;
    int ph = 40;

    // 擦除旧图片区域（黑色背景）
    LCD_FillFast(px, py, px + pw, py + ph, BLACK);

    // 更新位置
    px += pvx;
    py += pvy;

    // 边界反弹
    if (px <= 0) { px = 0; pvx = -pvx; }
    if (px + pw >= LCD_W) { px = LCD_W - pw; pvx = -pvx; }
    if (py <= 0) { py = 0; pvy = -pvy; }
    if (py + ph >= LCD_H) { py = LCD_H - ph; pvy = -pvy; }

    // 用高速 DMA 绘制图片
    LCD_ShowPictureFast(px, py, pw, ph, gImage_1);
}
// ============ 主函数 ============
void app_main(void)
{
    esp_task_wdt_deinit();
    srand(12345);

    LCD_Init();
    init_tables();
    init_tunnel();
    LCD_FillFast(0, 0, LCD_W, LCD_H, BLACK);

    int64_t last_time = esp_timer_get_time();
    int frame_cnt = 0;
    u8 fps = 0;
    u8 mode = 0;
    u8 mode_timer = 0;
    u8 time = 0;

    while (1)
    {
        int64_t now = esp_timer_get_time();
        frame_cnt++;
        time++;

        if (now - last_time >= 1000000)
        {
            fps = frame_cnt;
            draw_fps(fps);
            frame_cnt = 0;
            last_time = now;
            mode_timer++;
            if (mode_timer >= 5) // 每 5 秒切效果
            {
                mode_timer = 0;
                mode = (mode + 1) % 5;
                memset(fire_buf, 0, sizeof(fire_buf));
            }
        }


        switch (mode)
        {
        case 0: render_plasma(time);  LCD_ShowPictureFast(0, 0, LCD_W, LCD_H, rgb_buf); break;
        case 1: render_tunnel(time);  LCD_ShowPictureFast(0, 0, LCD_W, LCD_H, rgb_buf); break;
        case 2: render_moire(time);   LCD_ShowPictureFast(0, 0, LCD_W, LCD_H, rgb_buf); break;
        case 3: render_fire(time);    LCD_ShowPictureFast(0, 0, LCD_W, LCD_H, rgb_buf); break;
        case 4: anim_pic_bounce();    break;
        }

        vTaskDelay(1);
    }
}