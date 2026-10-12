#include "Beep.h"
#include "Delay.h"
#include "FingerPrint.h"
#include "Marix_Key.h"
#include "OLED.h"
#include "RC522.h"
#include "ST7735.h"
#include "Timer.h"
#include "UART_Fingerprint.h"
#include "W64Q25.h"
#include "audio.h"
#include "bluetooth.h"
#include "picture.h" //里面通过宏定义方式让它不参与编译，在需要导入图片的时候再使用
#include "servo.h"
#include "stm32f10x.h" // Device header
#include "string.h"

#define CLOSE_DOOR SERVO_SetAngle(0)
#define OPEN_DOOR SERVO_SetAngle(180)

#define Fingerprint_count 6
#define Card_count 6

typedef enum
{
    ADMINISTRATOR = 0, // 管理员界面
    PASSWORD,          // 密码解锁
    FINGERPRINT,       // 指纹解锁
    NFC,               // NFC解锁
    BLUETOOTH,         // 蓝牙解锁
} LCD_Status;

typedef enum
{
    DOOR_OPEN,
    DOOR_CLOSE
} DOOR_Status;

typedef enum
{
    REFRASH = 0,
    HOLD
} MENU_REFRESH_flag;
typedef enum
{
    FP_EMPTY = 0, // 这个编号还没有录入指纹
    FP_EXIST      // 这个编号已经录入指纹
} Fingerprint_Status;
typedef enum
{
    CARD_EMPTY = 0, // 这个编号还没有录入NFC
    CARD_EXIST      // 这个编号已经录入NFC
} Card_Status;

typedef enum
{
    AD_ON = 0,
    AD_OFF
} Audio_Voice_Status;

typedef enum
{
    BEEF_ON = 0,
    BEEF_OFF
} Beep_Voice_Status;

// 定义单张卡片的完整信息
typedef struct
{
    Card_Status status; // 状态：是否已录入
    uint32_t card_id;   // 录入的卡片 32位 UID
} Card_Info;

// 卡片在 Flash 中持久化存储的数据结构体（魔数 + 6张卡片信息）
typedef struct
{
    uint16_t magic;              // 校验魔数（CARD_STORAGE_MAGIC = 0xA55A）
    Card_Info cards[Card_count]; // 6张卡片的完整信息（包含录入状态和UID）
} Card_Flash_Data;

uint8_t key_num;
LCD_Status lcd_status = ADMINISTRATOR;
DOOR_Status door_status = DOOR_CLOSE;
MENU_REFRESH_flag menu_refresh_flag = REFRASH;
Audio_Voice_Status audio_voice_status = AD_OFF;
Beep_Voice_Status beep_voice_status = BEEF_ON;
Fingerprint_Status fp_status[Fingerprint_count] = {FP_EMPTY}; // 定义包含 6 个指纹状态的数组
Card_Status card_status[Card_count] = {CARD_EMPTY};           // 定义包含 6 个NFC状态的数组
Card_Info card_info[Card_count] = {0};                        // 定义包含 6 个卡片信息的数组，并初始化为空
uint8_t card_index = 0;
uint16_t password = 1234;
uint16_t administrator_password = 1234;
const uint32_t POW10[10] = {
    1, 10, 100, 1000, 10000,
    100000, 1000000, 10000000, 100000000, 1000000000};
uint8_t keyPress_value_use = 0;
uint8_t picture_container[4050] = {0};
uint8_t volume_level = 23; // 提示音音量，最大值为30

void Menu_Interface(void);

void Password_Interface(void);
void NFC_Interface(void);
void FingerPrint_Interface(void);
void Bluetooth_Interface(void);

void administratior(void);
void administratior_password(void);
void administratior_change_admin_password(void);
void administratior_change_unlock_password(void);
void administratior_sound(void);
void administratior_volume(void);
void administratior_fingerprint(void);
void administratior_fingerprint_enroll(void);
void administratior_fingerprint_delete(void);
void administratior_card(void);
void administratior_card_enroll(void);
void administratior_card_delete(void);

void Card_SaveToFlash(void);
void Card_LoadFromFlash(void);

int main(void)
{
    LCD_Init();
    Matrix_Key_Init();
    SERVO_Init();
    RC522_Init();
    TIMER_Init();
    Beep_Init();
    JM101_Init();
    JDY31_Init(9600);
    Audio_Init();
    W64Q25_Init();
    Card_LoadFromFlash(); // 开机从 W25Q64 Flash 中读取已录入的卡片信息（掉电不丢失）

    /*将图片数据放进W64Q25之后进行注释，且将图片信息也注释防止占用大量flash空间*/
    // W64Q25_SectorProgram(PICTURE_ADMINISTRATOR_45X45, gImage_administrator45_45, 4050);
    // W64Q25_SectorProgram(PICTURE_BLUETOOTH_45X45, gImage_bluetooth45_45, 4050);
    // W64Q25_SectorProgram(PICTURE_PASSWORD_45X45, gImage_password45_45, 4050);
    // W64Q25_SectorProgram(PICTURE_CARD_45X45, gImage_card45_45, 4050);
    // W64Q25_SectorProgram(PICTURE_FINGERPRINT_45X45, gImage_fingerprint45_45, 4050);
    LCD_FillColor(WHITE);
    Audio_SetVolume(volume_level); // 设置音量，范围0～30
    Audio_SetPlayMode(AUDIO_PLAY_MODE_SINGLE_STOP, 0);
    Audio_PlayTrack(10);

    // 把主菜单界面放进

    while (1)
    {
        if (menu_refresh_flag == REFRASH)
        {
            // LCD上显示出菜单，并且等待按下按键
            Menu_Interface();
            menu_refresh_flag = HOLD; // 刷新完了就设置成不刷新状态
        }
        // LCD根据door_status的值，显示开锁状态/关锁状态
        if (key_flag == 1)
        {
            key_flag = 0;                        // 接收到按键更新标志，要立即复位标志，然后后面去读keyPress_value就好了
            keyPress_value_use = keyPress_value; // 把它读出来
            keyPress_value = 50;                 // 及时清理

            // 根据不同的键值进入不同的界面，设置状态标志，进入对应的函数功能里
            switch (keyPress_value_use)
            {
            case 1: {
                lcd_status = ADMINISTRATOR;
                Audio_PlayTrack(audio_aministrator);
                // 进入封装好的函数里面
                administratior();
                break;
            }
            case 2: {
                Audio_PlayTrack(audio_password);
                lcd_status = PASSWORD;
                // 进入密码验证界面
                Password_Interface();
                break;
            }
            case 3: {
                Audio_PlayTrack(audio_fingerprint);
                FingerPrint_Interface();
                break;
            }
            case 4: {
                Audio_PlayTrack(audio_card);
                NFC_Interface();
                break;
            }
            case 5: {
                Audio_PlayTrack(audio_bluetooth);
                Bluetooth_Interface();
                break;
            }
            case 12: {

                break;
            }
            case 13: // 关锁键
            {
                // 操作舵机
                // LCD显示关锁状态
                door_status = DOOR_CLOSE;
                audio_voice_status = AD_ON; // 打开“关锁”状态提示语音
                break;
            }

                // ...待补充

            default: {
                break;
            }
            }
            /**根据标志位，来控制舵机  */
            if ((door_status == DOOR_CLOSE) && (audio_voice_status == AD_ON))
            {
                CLOSE_DOOR;
                Audio_PlayTrack(audio_close_door);
                audio_voice_status = AD_OFF;
            }
            else if ((door_status == DOOR_OPEN) && (audio_voice_status == AD_ON))
            {
                OPEN_DOOR;
                Audio_PlayTrack(audio_open_door);
                audio_voice_status = AD_OFF;
            }
            else
            {
            }
            menu_refresh_flag = REFRASH;
        }
    }
}

void administratior(void)
{
    uint16_t UserInput_current_password = 0;
    // LCD显示要求输入当前密码
    // 等待用户按键输入
    LCD_FillColor(WHITE);
    // 顶部标题栏（深石墨灰底色 + 香槟金字 "管理员"）
    LCD_FillRect(0, 0, 128, 20, RGB565_DARK_SLATE);
    LCD_DrawGBKString16x16(25, 2, administrator_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
    LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);

    // 中间绘制管理员图标（45x45），居中放置: (128 - 45) / 2 ≈ 42
    // LCD_DrawImageRGB565(42, 25, 45, 45, gImage_administrator45_45);
    W64Q25_ReadData(PICTURE_ADMINISTRATOR_45X45, picture_container, 4050);
    LCD_DrawImageRGB565(45, 25, 45, 45, picture_container);
    /*先进行密码验证，否则没有权限进入 */
    LCD_DrawGBKString16x16(0, 73, enter_admin_password_GBK, RGB565_HERMES_ORANGE, WHITE);
    LCD_DrawUInt32(48, 93, 0, 4, RGB565_BLACK, RGB565_WHITE);
    for (uint8_t i = 0; i < 4; i++)
    {
        /* 等待“下一次新的按键按下” */
        while (key_flag == 0)
        {
        }
        /* 读取并消费该按键事件 */
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        if (key_num <= 9)
        {
            UserInput_current_password += key_num * POW10[4 - i - 1];
            // LCD根据i的值，随着用户输入数字同步显示在LCD上
            LCD_DrawUInt32(48, 93, UserInput_current_password, 4, RGB565_BLACK, RGB565_WHITE);
        }
        else if (key_num == 11) // 返回键
        {

            return;
        }
        else
        {
            i--; // 非法输入，重新等待用户按键输入
        }
    }
    if (UserInput_current_password == administrator_password)
    {
        Audio_PlayTrack(audio_password_pass);
        LCD_FillRect(0, 72, 128, 56, WHITE);

        // 验证通过，LCD等待新密码
        LCD_DrawGBKString16x16(32, 76, password_right_GBK, RGB565_EMERALD_GREEN, WHITE);
        LCD_DrawGBKString16x16(8, 96, enter_adminisrator_GBK, RGB565_DARK_SLATE, WHITE);
        Delay_ms(1500);
    }
    else
    {
        Audio_PlayTrack(audio_password_error);
        LCD_FillRect(0, 72, 128, 56, WHITE);

        // 密码错误，请重试
        LCD_DrawGBKString16x16(24, 76, password_error_GBK, RGB565_CORAL_RED, WHITE);
        LCD_DrawGBKString16x16(40, 96, try_again_GBK, RGB565_CORAL_RED, WHITE);
        Delay_ms(1500);
        return;
    }
    while (1)
    {
        LCD_FillColor(RGB565_BLACK);
        LCD_FillRect(0, 0, 128, 20, RGB565_MIDNIGHT_BLUE);
        LCD_DrawGBKString16x16(25, 2, administrator_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_MIDNIGHT_BLUE);
        LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);

        // LCD打印四个选项
        LCD_DrawGBKString16x16(0, 25, changePassword_GBK, RGB565_WHITE, RGB565_BLACK);
        LCD_DrawGBKString16x16(0, 44, fingerprint_manage_GBK, RGB565_WHITE, RGB565_BLACK);
        LCD_DrawGBKString16x16(0, 63, Card_manage_GBK, RGB565_WHITE, RGB565_BLACK);
        LCD_DrawGBKString16x16(0, 82, sound_manage_four_GBK, RGB565_WHITE, RGB565_BLACK);

        LCD_DrawLine(0, 106, 127, 106, RGB565_DARK_GRAY);
        LCD_DrawString8x16(24, 110, "[B]", RGB565_BORDER_GRAY, RGB565_BLACK);
        LCD_DrawGBKString16x16(56, 110, return_GBK, RGB565_BORDER_GRAY, RGB565_BLACK);

        // 等待用户按键输入
        while (key_flag == 0)
        {
        }
        /* 读取并消费该按键事件 */
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        // 根据按键值进入不同的设置页面
        switch (key_num)
        {
        case 1: // 修改密码
        {
            administratior_password();
            break;
        }
        case 2: // 指纹管理
        {
            administratior_fingerprint();
            break;
        }
        case 3: // 刷卡管理
        {
            administratior_card();
            break;
        }
        case 4: // 声音管理
        {
            administratior_sound();
            break;
        }
        case 11: // 返回键
        {
            return;
        }
        }
    }
}

void administratior_password(void)
{
    while (1)
    {
        LCD_FillColor(RGB565_BLACK);
        LCD_FillRect(0, 0, 128, 20, RGB565_MIDNIGHT_BLUE);
        LCD_DrawGBKString16x16(32, 2, changePassword1_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_MIDNIGHT_BLUE);
        LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);

        LCD_DrawGBKString16x16(0, 35, change_admin_password_one_GBK, RGB565_WHITE, RGB565_BLACK);
        LCD_DrawGBKString16x16(0, 65, change_unlock_password_two_GBK, RGB565_WHITE, RGB565_BLACK);

        LCD_DrawLine(0, 106, 127, 106, RGB565_DARK_GRAY);
        LCD_DrawString8x16(24, 110, "[B]", RGB565_BORDER_GRAY, RGB565_BLACK);
        LCD_DrawGBKString16x16(56, 110, return_GBK, RGB565_BORDER_GRAY, RGB565_BLACK);

        while (key_flag == 0)
        {
        }
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        switch (key_num)
        {
        case 1:
            administratior_change_admin_password();
            break;
        case 2:
            administratior_change_unlock_password();
            break;
        case 11:
            return;
        default:
            break;
        }
    }
}

void administratior_change_admin_password(void)
{
    uint16_t UserInput_new_password = 0;
    LCD_FillColor(WHITE);
    // 顶部标题栏
    LCD_FillRect(0, 0, 128, 20, RGB565_DARK_SLATE);
    LCD_DrawGBKString16x16(8, 2, change_admin_password_title_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
    LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);

    // 中间放置密码图标 45x45
    W64Q25_ReadData(PICTURE_PASSWORD_45X45, picture_container, 4050);
    LCD_DrawImageRGB565(45, 25, 45, 45, picture_container);

    // 提示 "输入新密码"
    LCD_DrawGBKString16x16(24, 73, enter_new_password_no_colon_GBK, RGB565_HERMES_ORANGE, WHITE);
    LCD_DrawUInt32(48, 93, 0, 4, RGB565_BLACK, RGB565_WHITE);

    for (uint8_t i = 0; i < 4; i++)
    {
        /* 等待“下一次新的按键按下” */
        while (key_flag == 0)
        {
        }
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        if (key_num <= 9)
        {
            UserInput_new_password += key_num * POW10[4 - i - 1];
            LCD_DrawUInt32(48, 93, UserInput_new_password, 4, BLACK, WHITE);
            if (i == 3) // 修改密码成功
            {
                administrator_password = UserInput_new_password;
                LCD_FillRect(0, 72, 128, 56, WHITE);
                LCD_DrawString8x16(8, 76, "Success! New is:", RGB565_EMERALD_GREEN, WHITE);
                LCD_DrawUInt32(48, 96, administrator_password, 4, RGB565_EMERALD_GREEN, WHITE);
                Delay_ms(2000);
                return;
            }
        }
        else if (key_num == 11) // 返回键：取消修改，原密码保持不变
        {
            return;
        }
        else
        {
            i--; // 非法输入，重新等待有效数字输入
        }
    }
}

void administratior_change_unlock_password(void)
{
    uint16_t UserInput_new_password = 0;
    LCD_FillColor(WHITE);
    // 顶部标题栏
    LCD_FillRect(0, 0, 128, 20, RGB565_DARK_SLATE);
    LCD_DrawGBKString16x16(16, 2, change_unlock_password_title_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
    LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);

    // 中间放置密码图标 45x45
    W64Q25_ReadData(PICTURE_PASSWORD_45X45, picture_container, 4050);
    LCD_DrawImageRGB565(45, 25, 45, 45, picture_container);

    // 提示 "输入新密码"
    LCD_DrawGBKString16x16(24, 73, enter_new_password_no_colon_GBK, RGB565_HERMES_ORANGE, WHITE);
    LCD_DrawUInt32(48, 93, 0, 4, RGB565_BLACK, RGB565_WHITE);

    for (uint8_t i = 0; i < 4; i++)
    {
        /* 等待“下一次新的按键按下” */
        while (key_flag == 0)
        {
        }
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        if (key_num <= 9)
        {
            UserInput_new_password += key_num * POW10[4 - i - 1];
            LCD_DrawUInt32(48, 93, UserInput_new_password, 4, BLACK, WHITE);
            if (i == 3) // 修改密码成功
            {
                password = UserInput_new_password;
                LCD_FillRect(0, 72, 128, 56, WHITE);
                LCD_DrawString8x16(8, 76, "Success! New is:", RGB565_EMERALD_GREEN, WHITE);
                LCD_DrawUInt32(48, 96, password, 4, RGB565_EMERALD_GREEN, WHITE);
                Delay_ms(2000);
                return;
            }
        }
        else if (key_num == 11) // 返回键：取消修改，原密码保持不变
        {
            return;
        }
        else
        {
            i--; // 非法输入，重新等待有效数字输入
        }
    }
}

void administratior_sound(void)
{
    while (1)
    {
        LCD_FillColor(RGB565_BLACK);
        LCD_FillRect(0, 0, 128, 20, RGB565_MIDNIGHT_BLUE);
        LCD_DrawGBKString16x16(32, 2, sound_manage_title_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_MIDNIGHT_BLUE);
        LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);

        if (beep_voice_status == BEEF_ON)
        {
            LCD_DrawGBKString16x16(0, 35, key_beep_on_one_GBK, RGB565_WHITE, RGB565_BLACK);
        }
        else
        {
            LCD_DrawGBKString16x16(0, 35, key_beep_off_one_GBK, RGB565_WHITE, RGB565_BLACK);
        }

        LCD_DrawGBKString16x16(0, 65, prompt_volume_two_GBK, RGB565_WHITE, RGB565_BLACK);

        LCD_DrawLine(0, 106, 127, 106, RGB565_DARK_GRAY);
        LCD_DrawString8x16(24, 110, "[B]", RGB565_BORDER_GRAY, RGB565_BLACK);
        LCD_DrawGBKString16x16(56, 110, return_GBK, RGB565_BORDER_GRAY, RGB565_BLACK);

        while (key_flag == 0)
        {
        }
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        if (key_num == 1)
        {
            if (beep_voice_status == BEEF_OFF)
            {
                beep_voice_status = BEEF_ON;
                Beep_SetKeyVoice(1);
                Beep_Flash(); // 切换到开启状态时立即响一下以作为回应
            }
            else
            {
                beep_voice_status = BEEF_OFF;
                Beep_SetKeyVoice(0);
            }
        }
        else if (key_num == 2)
        {
            administratior_volume();
        }
        else if (key_num == 11)
        {
            return;
        }
    }
}

void administratior_volume(void)
{
    LCD_FillColor(RGB565_BLACK);
    LCD_FillRect(0, 0, 128, 20, RGB565_MIDNIGHT_BLUE);
    LCD_DrawGBKString16x16(16, 2, prompt_volume_title_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_MIDNIGHT_BLUE);
    LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);

    LCD_DrawGBKString16x16(28, 26, volume_label_GBK, RGB565_WHITE, RGB565_BLACK);
    LCD_DrawChar8x16(60, 26, ':', RGB565_WHITE, RGB565_BLACK);
    LCD_DrawUInt32(72, 26, volume_level, 2, RGB565_EMERALD_GREEN, RGB565_BLACK);

    LCD_DrawString8x16(12, 46, "[1]", RGB565_BORDER_GRAY, RGB565_BLACK);
    LCD_DrawGBKString16x16(40, 46, increase_volume_GBK, RGB565_WHITE, RGB565_BLACK);

    LCD_DrawString8x16(12, 65, "[2]", RGB565_BORDER_GRAY, RGB565_BLACK);
    LCD_DrawGBKString16x16(40, 65, decrease_volume_GBK, RGB565_WHITE, RGB565_BLACK);

    LCD_DrawString8x16(12, 84, "[0]", RGB565_BORDER_GRAY, RGB565_BLACK);
    LCD_DrawGBKString16x16(40, 84, mute_GBK, RGB565_WHITE, RGB565_BLACK);

    LCD_DrawLine(0, 104, 127, 104, RGB565_DARK_GRAY);
    LCD_DrawString8x16(24, 108, "[B]", RGB565_BORDER_GRAY, RGB565_BLACK);
    LCD_DrawGBKString16x16(56, 108, return_GBK, RGB565_BORDER_GRAY, RGB565_BLACK);

    while (1)
    {
        while (key_flag == 0)
        {
        }
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        if (key_num == 1) // 增高音量
        {
            if (volume_level < 30)
            {
                volume_level++;
            }
            Audio_SetVolume(volume_level);
            LCD_DrawUInt32(72, 26, volume_level, 2, RGB565_EMERALD_GREEN, RGB565_BLACK);
            Audio_PlayTrack(10);
        }
        else if (key_num == 2) // 降低音量
        {
            if (volume_level > 0)
            {
                volume_level--;
            }
            Audio_SetVolume(volume_level);
            LCD_DrawUInt32(72, 26, volume_level, 2, RGB565_EMERALD_GREEN, RGB565_BLACK);
            if (volume_level > 0)
            {
                Audio_PlayTrack(10);
            }
        }
        else if (key_num == 0) // 静音
        {
            volume_level = 0;
            Audio_SetVolume(volume_level);
            LCD_DrawUInt32(72, 26, volume_level, 2, RGB565_EMERALD_GREEN, RGB565_BLACK);
        }
        else if (key_num == 11) // B键返回
        {
            return;
        }
    }
}

void administratior_fingerprint(void)
{
    while (1)
    {
        LCD_FillColor(WHITE);
        // 顶部标题栏
        LCD_FillRect(0, 0, 128, 18, RGB565_DARK_SLATE);
        LCD_DrawGBKString16x16(32, 1, fingerprint_manage1_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
        LCD_DrawLine(0, 18, 127, 18, RGB565_DARK_GRAY);

        // 选项列表
        LCD_DrawGBKString16x16(24, 38, enroll_fingerprint_one_GBK, RGB565_DARK_SLATE, WHITE); // 一、录入指纹
        LCD_DrawGBKString16x16(24, 68, delete_fingerprint_two_GBK, RGB565_DARK_SLATE, WHITE); // 二、删除指纹

        LCD_DrawLine(0, 106, 127, 106, RGB565_DARK_GRAY);
        LCD_DrawString8x16(24, 110, "[B]", RGB565_BORDER_GRAY, WHITE);
        LCD_DrawGBKString16x16(56, 110, return_GBK, RGB565_BORDER_GRAY, WHITE);

        // 等待用户按键输入
        while (key_flag == 0)
        {
        }
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        if (key_num == 1)
        {
            administratior_fingerprint_enroll();
        }
        else if (key_num == 2)
        {
            administratior_fingerprint_delete();
        }
        else if (key_num == 11)
        {
            return;
        }
    }
}

void administratior_fingerprint_enroll(void)
{
    while (1)
    {
        LCD_FillColor(WHITE);
        // 顶部标题栏
        LCD_FillRect(0, 0, 128, 18, RGB565_DARK_SLATE);
        LCD_DrawGBKString16x16(32, 1, enroll_fingerprint_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
        LCD_DrawLine(0, 18, 127, 18, RGB565_DARK_GRAY);

        // 提示语纯白底 + 橙色文字
        LCD_DrawGBKString16x16(0, 20, enroll_fingerprintNum_GBK, RGB565_HERMES_ORANGE, WHITE);

        for (uint8_t i = 0; i < 6; i++)
        {
            LCD_DrawUInt32(4, i * 15 + 36, i + 1, 1, RGB565_DARK_SLATE, WHITE);
            LCD_DrawGBKString16x16(14, i * 15 + 36, num_GBK, RGB565_DARK_SLATE, WHITE);
        }
        for (uint8_t i = 0; i < 6; i++)
        {
            if (fp_status[i] == FP_EMPTY)
            {
                LCD_DrawGBKString16x16(38, i * 15 + 36, empty_GBK, RGB565_BORDER_GRAY, WHITE);
            }
            else
            {
                LCD_DrawGBKString16x16(38, i * 15 + 36, already_recorded_GBK, RGB565_EMERALD_GREEN, WHITE);
            }
        }
        // 等待用户按键输入
        while (key_flag == 0)
        {
        }
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        if (key_num >= 1 && key_num <= 6)
        {
            LCD_FillRect(0, 20, 128, 16, WHITE); // 清除原来的提示语
            LCD_DrawGBKString16x16(0, 20, press_finger_GBK, RGB565_HERMES_ORANGE, WHITE);

            LCD_FillRect(38, ((key_num - 1) * 15) + 36, 16 * 4, 15, WHITE);
            LCD_DrawGBKString16x16(38, ((key_num - 1) * 15) + 36, enrolling_fingerprint_GBK, RGB565_HERMES_ORANGE, WHITE); // 正在录入

            uint8_t enroll_result = JM101_Register(key_num - 1, 2, 0x0009);

            if (enroll_result != 0)
            {
                fp_status[key_num - 1] = FP_EXIST;
                LCD_FillRect(38, ((key_num - 1) * 15) + 36, 16 * 4, 15, WHITE);
                LCD_FillRect(0, 20, 128, 16, WHITE);
                LCD_DrawGBKString16x16(38, ((key_num - 1) * 15) + 36, enroll_succeed_GBK, RGB565_EMERALD_GREEN, WHITE); // 录入成功
                Delay_ms(1000);
            }
            else
            {
                LCD_FillRect(38, ((key_num - 1) * 15) + 36, 16 * 4, 15, WHITE);
                LCD_DrawGBKString16x16(38, ((key_num - 1) * 15) + 36, enroll_error_GBK, RGB565_CORAL_RED, WHITE); // 录入失败
                Delay_ms(1000);
            }
        }
        else if (key_num == 11)
        {
            return;
        }
    }
}

void administratior_fingerprint_delete(void)
{
    while (1)
    {
        LCD_FillColor(WHITE);
        // 顶部标题栏
        LCD_FillRect(0, 0, 128, 18, RGB565_DARK_SLATE);
        LCD_DrawGBKString16x16(32, 1, delete_fingerprint_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
        LCD_DrawLine(0, 18, 127, 18, RGB565_DARK_GRAY);

        // 提示语纯白底 + 橙色文字
        LCD_DrawGBKString16x16(0, 20, delete_fingerprintNum_GBK, RGB565_HERMES_ORANGE, WHITE);

        for (uint8_t i = 0; i < 6; i++)
        {
            LCD_DrawUInt32(4, i * 15 + 36, i + 1, 1, RGB565_DARK_SLATE, WHITE);
            LCD_DrawGBKString16x16(14, i * 15 + 36, num_GBK, RGB565_DARK_SLATE, WHITE);
        }
        for (uint8_t i = 0; i < 6; i++)
        {
            if (fp_status[i] == FP_EMPTY)
            {
                LCD_DrawGBKString16x16(38, i * 15 + 36, empty_GBK, RGB565_BORDER_GRAY, WHITE);
            }
            else
            {
                LCD_DrawGBKString16x16(38, i * 15 + 36, already_recorded_GBK, RGB565_EMERALD_GREEN, WHITE);
            }
        }

        // 右侧提示 [0] 全部删除 与 [B] 返回
        LCD_DrawLine(86, 36, 86, 122, RGB565_BORDER_GRAY);
        LCD_DrawString8x16(94, 40, "[0]", RGB565_CORAL_RED, WHITE);
        LCD_DrawChinese16x16(88, 58, delete_allcard_GBK[0], delete_allcard_GBK[1], RGB565_CORAL_RED, WHITE);  // 全
        LCD_DrawChinese16x16(104, 58, delete_allcard_GBK[2], delete_allcard_GBK[3], RGB565_CORAL_RED, WHITE); // 部
        LCD_DrawChinese16x16(88, 76, delete_allcard_GBK[4], delete_allcard_GBK[5], RGB565_CORAL_RED, WHITE);  // 删
        LCD_DrawChinese16x16(104, 76, delete_allcard_GBK[6], delete_allcard_GBK[7], RGB565_CORAL_RED, WHITE); // 除
        LCD_DrawString8x16(92, 102, "[B]", RGB565_BORDER_GRAY, WHITE);

        // 等待用户按键输入
        while (key_flag == 0)
        {
        }
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        if (key_num >= 1 && key_num <= 6)
        {
            JM101_Delete(key_num - 1, 1);
            fp_status[key_num - 1] = FP_EMPTY;

            LCD_FillRect(38, ((key_num - 1) * 15) + 36, 16 * 4, 15, WHITE);
            LCD_DrawGBKString16x16(38, ((key_num - 1) * 15) + 36, delete_secceed_GBK, RGB565_EMERALD_GREEN, WHITE); // 删除成功
            Delay_ms(500);

            LCD_FillRect(38, ((key_num - 1) * 15) + 36, 16 * 4, 15, WHITE);
            LCD_DrawGBKString16x16(38, ((key_num - 1) * 15) + 36, empty_GBK, RGB565_BORDER_GRAY, WHITE); // 空
            Delay_ms(500);
        }
        else if (key_num == 0) // 全部删除
        {
            JM101_DeleteAll();
            for (uint8_t i = 0; i < Fingerprint_count; i++)
            {
                fp_status[i] = FP_EMPTY;
            }
            LCD_FillRect(0, 20, 128, 16, WHITE);
            LCD_DrawGBKString16x16(32, 20, delete_secceed_GBK, RGB565_EMERALD_GREEN, WHITE);
            Delay_ms(800);
        }
        else if (key_num == 11)
        {
            return;
        }
    }
}

void administratior_card(void)
{
    while (1)
    {
        LCD_FillColor(WHITE);
        // 顶部标题栏
        LCD_FillRect(0, 0, 128, 18, RGB565_DARK_SLATE);
        LCD_DrawGBKString16x16(32, 1, card_manage1_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
        LCD_DrawLine(0, 18, 127, 18, RGB565_DARK_GRAY);

        // 选项列表
        LCD_DrawGBKString16x16(24, 38, enroll_card_one_GBK, RGB565_DARK_SLATE, WHITE); // 一、录入卡片
        LCD_DrawGBKString16x16(24, 68, delete_card_two_GBK, RGB565_DARK_SLATE, WHITE); // 二、删除卡片

        LCD_DrawLine(0, 106, 127, 106, RGB565_DARK_GRAY);
        LCD_DrawString8x16(24, 110, "[B]", RGB565_BORDER_GRAY, WHITE);
        LCD_DrawGBKString16x16(56, 110, return_GBK, RGB565_BORDER_GRAY, WHITE);

        // 等待用户按键输入
        while (key_flag == 0)
        {
        }
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        if (key_num == 1)
        {
            administratior_card_enroll();
        }
        else if (key_num == 2)
        {
            administratior_card_delete();
        }
        else if (key_num == 11)
        {
            return;
        }
    }
}

void administratior_card_enroll(void)
{
    uint8_t uid[4] = {0};
    uint8_t atqa[2] = {0};
    RC522_Status status;
    RC522_Status old_status;

    while (1)
    {
        LCD_FillColor(WHITE);
        // 顶部标题栏
        LCD_FillRect(0, 0, 128, 18, RGB565_DARK_SLATE);
        LCD_DrawGBKString16x16(32, 1, enroll_card_title_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
        LCD_DrawLine(0, 18, 127, 18, RGB565_DARK_GRAY);

        // 提示语纯白底 + 橙色文字
        LCD_DrawGBKString16x16(0, 20, enroll_cardNum_GBK, RGB565_HERMES_ORANGE, WHITE);

        for (uint8_t i = 0; i < 6; i++)
        {
            LCD_DrawUInt32(4, i * 15 + 36, i + 1, 1, RGB565_DARK_SLATE, WHITE);
            LCD_DrawGBKString16x16(14, i * 15 + 36, num_GBK, RGB565_DARK_SLATE, WHITE);
        }
        for (uint8_t i = 0; i < 6; i++)
        {
            if (card_info[i].status == CARD_EMPTY)
            {
                LCD_DrawGBKString16x16(38, i * 15 + 36, empty_GBK, RGB565_BORDER_GRAY, WHITE);
            }
            else
            {
                LCD_DrawGBKString16x16(38, i * 15 + 36, already_recorded_GBK, RGB565_EMERALD_GREEN, WHITE);
            }
        }
        // 等待用户按键输入
        while (key_flag == 0)
        {
        }
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        if (key_num >= 1 && key_num <= 6)
        {
            LCD_FillRect(38, ((key_num - 1) * 15) + 36, 16 * 4, 15, WHITE);
            LCD_FillRect(0, 20, 128, 16, WHITE);                                                                           // 清除原来的卡片状态
            LCD_DrawGBKString16x16(38, ((key_num - 1) * 15) + 36, enrolling_fingerprint_GBK, RGB565_HERMES_ORANGE, WHITE); // 正在录入
            LCD_DrawGBKString16x16(24, 20, enroll_card_GBK, RGB565_HERMES_ORANGE, WHITE);                                  // 请录入卡片

            while (1)
            {
                if (key_flag == 1)
                {
                    uint8_t k = keyPress_value;
                    keyPress_value = KEY_NONE;
                    key_flag = 0;
                    if (k == 11) // B键取消当前录入
                    {
                        break;
                    }
                }
                status = RC522_ReadUid4(uid, atqa);
                if (old_status != status)
                {
                    if (status == RC522_OK)
                    {
                        uint32_t result = ((uint32_t)uid[0] << 24U) | ((uint32_t)uid[1] << 16U) |
                                          ((uint32_t)uid[2] << 8U) | (uint32_t)uid[3];
                        Beep_Flash(); // 提醒用户录入成功
                        LCD_FillRect(38, ((key_num - 1) * 15) + 36, 16 * 4, 15, WHITE);
                        LCD_FillRect(0, 20, 128, 16, WHITE);
                        LCD_DrawGBKString16x16(38, ((key_num - 1) * 15) + 36, enroll_succeed1_GBK, RGB565_EMERALD_GREEN, WHITE); // 录入成功！
                        card_info[key_num - 1].card_id = result;
                        card_info[key_num - 1].status = CARD_EXIST;
                        card_status[key_num - 1] = CARD_EXIST;
                        Card_SaveToFlash(); // 录入成功，同步保存到 W25Q64 Flash 中（掉电不丢失）
                        Delay_ms(1000);
                        break;
                    }
                    else if (status != RC522_TIMEOUT)
                    {
                        LCD_FillRect(38, ((key_num - 1) * 15) + 36, 16 * 4, 15, WHITE);
                        LCD_DrawGBKString16x16(38, ((key_num - 1) * 15) + 36, enroll_error_GBK, RGB565_CORAL_RED, WHITE); // 录入失败
                        Delay_ms(1000);
                        break;
                    }
                    old_status = status;
                    Delay_ms(20);
                }
            }
        }
        else if (key_num == 11)
        {
            return;
        }
    }
}

void administratior_card_delete(void)
{
    while (1)
    {
        LCD_FillColor(WHITE);
        // 顶部标题栏
        LCD_FillRect(0, 0, 128, 18, RGB565_DARK_SLATE);
        LCD_DrawGBKString16x16(32, 1, delete_card_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
        LCD_DrawLine(0, 18, 127, 18, RGB565_DARK_GRAY);

        // 提示语纯白底 + 橙色文字
        LCD_DrawGBKString16x16(0, 20, delete_cardNum_GBK, RGB565_HERMES_ORANGE, WHITE);

        for (uint8_t i = 0; i < 6; i++)
        {
            LCD_DrawUInt32(4, i * 15 + 36, i + 1, 1, RGB565_DARK_SLATE, WHITE);
            LCD_DrawGBKString16x16(14, i * 15 + 36, num_GBK, RGB565_DARK_SLATE, WHITE);
        }
        for (uint8_t i = 0; i < 6; i++)
        {
            if (card_info[i].status == CARD_EMPTY)
            {
                LCD_DrawGBKString16x16(38, i * 15 + 36, empty_GBK, RGB565_BORDER_GRAY, WHITE);
            }
            else
            {
                LCD_DrawGBKString16x16(38, i * 15 + 36, already_recorded_GBK, RGB565_EMERALD_GREEN, WHITE);
            }
        }

        // 右侧提示 [0] 全部删除 与 [B] 返回
        LCD_DrawLine(86, 36, 86, 122, RGB565_BORDER_GRAY);
        LCD_DrawString8x16(94, 40, "[0]", RGB565_CORAL_RED, WHITE);
        LCD_DrawChinese16x16(88, 58, delete_allcard_GBK[0], delete_allcard_GBK[1], RGB565_CORAL_RED, WHITE);  // 全
        LCD_DrawChinese16x16(104, 58, delete_allcard_GBK[2], delete_allcard_GBK[3], RGB565_CORAL_RED, WHITE); // 部
        LCD_DrawChinese16x16(88, 76, delete_allcard_GBK[4], delete_allcard_GBK[5], RGB565_CORAL_RED, WHITE);  // 删
        LCD_DrawChinese16x16(104, 76, delete_allcard_GBK[6], delete_allcard_GBK[7], RGB565_CORAL_RED, WHITE); // 除
        LCD_DrawString8x16(92, 102, "[B]", RGB565_BORDER_GRAY, WHITE);

        // 等待用户按键输入
        while (key_flag == 0)
        {
        }
        key_num = keyPress_value;
        keyPress_value = KEY_NONE;
        key_flag = 0;

        if (key_num >= 1 && key_num <= 6)
        {
            card_info[key_num - 1].card_id = 0;
            card_info[key_num - 1].status = CARD_EMPTY;
            card_status[key_num - 1] = CARD_EMPTY;
            Card_SaveToFlash(); // 单张删除成功，同步更新到 W25Q64 Flash

            LCD_FillRect(38, ((key_num - 1) * 15) + 36, 16 * 4, 15, WHITE);
            LCD_DrawGBKString16x16(38, ((key_num - 1) * 15) + 36, delete_secceed_GBK, RGB565_EMERALD_GREEN, WHITE); // 删除成功
            Delay_ms(500);

            LCD_FillRect(38, ((key_num - 1) * 15) + 36, 16 * 4, 15, WHITE);
            LCD_DrawGBKString16x16(38, ((key_num - 1) * 15) + 36, empty_GBK, RGB565_BORDER_GRAY, WHITE); // 空
            Delay_ms(500);
        }
        else if (key_num == 0) // 全部删除
        {
            for (uint8_t i = 0; i < Card_count; i++)
            {
                card_info[i].card_id = 0;
                card_info[i].status = CARD_EMPTY;
                card_status[i] = CARD_EMPTY;
            }
            Card_SaveToFlash(); // 全部清空，同步更新到 W25Q64 Flash
            LCD_FillRect(0, 20, 128, 16, WHITE);
            LCD_DrawGBKString16x16(32, 20, delete_secceed_GBK, RGB565_EMERALD_GREEN, WHITE);
            Delay_ms(800);
        }
        else if (key_num == 11)
        {
            return;
        }
    }
}

void Password_Interface(void)
{
    uint8_t try_again_flag = 0;
    uint16_t user_password = 0;
    while (1)
    {
        /*********LCD显示********** */
        if (try_again_flag == 0)
        {
            // “请输入密码”
            LCD_FillColor(WHITE);
            // LCD_DrawImageRGB565(45, 25, 45, 45, gImage_password45_45);
            W64Q25_ReadData(PICTURE_PASSWORD_45X45, picture_container, 4050);
            LCD_DrawImageRGB565(45, 25, 45, 45, picture_container);
            LCD_DrawString8x16(36, 5, "PASSWOED", RGB565_BLACK, RGB565_WHITE);
            LCD_DrawString8x16(13, 73, "Enter password", RGB565_BLACK, RGB565_WHITE);
        }
        LCD_DrawUInt32(48, 89, 0, 4, RGB565_BLACK, RGB565_WHITE);
        /********接收密码********** */
        for (uint8_t i = 0; i < 4; i++)
        {
            /* 等待“下一次新的按键按下” */
            while (key_flag == 0)
            {
            }
            /* 读取并消费该按键事件 */
            key_num = keyPress_value;
            keyPress_value = KEY_NONE;
            key_flag = 0;

            if (key_num <= 9)
            {
                user_password += key_num * POW10[4 - i - 1];
                // LCD根据i的值，随着用户输入数字同步显示在LCD上
                LCD_DrawUInt32(48, 89, user_password, 4, RGB565_BLACK, RGB565_WHITE);
            }
            else if (key_num == 11) // 返回键
            {

                return;
            }
        }
        if (user_password == password)
        {
            // LCD显示密码成功，已开锁
            // 舵机转动
            //  开锁状态
            door_status = DOOR_OPEN;
            Audio_PlayTrack(audio_password_pass);
            LCD_FillRect(1, 106, 127, 16, WHITE);
            LCD_DrawString8x16(1, 106, "PASSED!OPEN", BLACK, WHITE);
            for (uint8_t i = 0; i < 4; i++)
            {
                LCD_DrawString8x16(88 + i * 8, 106, ".", BLACK, WHITE);
                Delay_ms(300);
            }
            audio_voice_status = AD_ON; // 打开“开关锁”状态提示语音
            return;
        }
        else
        {
            door_status = DOOR_CLOSE;
            Audio_PlayTrack(audio_password_error);
            // LCD显示密码错误，请重新输入
            LCD_DrawString8x16(1, 106, "error,try again", BLACK, WHITE);
            try_again_flag = 1;
            LCD_FillRect(48, 89, 32, 16, WHITE);
            user_password = 0;
        }
    }
}

void FingerPrint_Interface(void)
{
    // // 1. 全屏黑色底色
    // LCD_FillColor(RGB565_BLACK);
    // // 2. 顶部标题栏（午夜深蓝底色 + 香槟金字）
    // LCD_FillRect(0, 0, 128, 20, RGB565_MIDNIGHT_BLUE);
    // LCD_DrawGBKString16x16(40, 2, MenuString_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_MIDNIGHT_BLUE);
    // LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);

    LCD_FillColor(WHITE);
    LCD_FillRect(0, 0, 128, 20, RGB565_DARK_SLATE);
    // LCD_DrawImageRGB565(45, 25, 45, 45, gImage_fingerprint45_45);
    W64Q25_ReadData(PICTURE_FINGERPRINT_45X45, picture_container, 4050);
    LCD_DrawImageRGB565(45, 25, 45, 45, picture_container);
    LCD_DrawGBKString16x16(40, 2, fingerprint1_string_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
    LCD_DrawGBKString16x16(24, 73, press_finger_GBK, RGB565_WARNING_YELLOW, WHITE);

    LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);
    uint8_t exam_result = 0;
    while (1)
    {
        exam_result = JM101_Verify(0xFFFF, 2, 0x0000);
        if (exam_result != 0)
        {
            door_status = DOOR_OPEN;
            Audio_PlayTrack(audio_fingerprint_pass);
            Beep_Flash();
            LCD_FillRect(1, 106, 127, 16, WHITE);
            LCD_DrawGBKString16x16(1, 91, verified_pass_GBK, BLACK, WHITE);
            LCD_DrawGBKString16x16(1, 106, open_the_door_GBK, BLACK, WHITE);
            for (uint8_t i = 0; i < 5; i++)
            {
                LCD_DrawString8x16(32 + i * 8, 107, ".", BLACK, WHITE);
                Delay_ms(290);
            }
            audio_voice_status = AD_ON; // 打开“开关锁”状态提示语音
            return;
        }
        else
        {
            door_status = DOOR_CLOSE;
            Audio_PlayTrack(audio_fingerprint_fail);
            LCD_FillRect(1, 106, 127, 16, WHITE);
            LCD_DrawGBKString16x16(1, 106, open_door_error_GBK, RED, WHITE);
            Delay_ms(1500);
            return;
        }
    }
}

void NFC_Interface(void)
{
    // 1. 全屏白色背景
    LCD_FillColor(WHITE);
    // 2. 顶部标题栏（深石墨灰底色 + 香槟金字）
    LCD_FillRect(0, 0, 128, 20, RGB565_DARK_SLATE);
    LCD_DrawGBKString16x16(32, 2, Card_string1_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
    LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);
    // 3. 中间绘制卡片图标（45x45），居中放置: (128 - 45) / 2 ≈ 42
    // LCD_DrawImageRGB565(42, 25, 45, 45, gImage_card45_45);
    W64Q25_ReadData(PICTURE_CARD_45X45, picture_container, 4050);
    LCD_DrawImageRGB565(45, 25, 45, 45, picture_container);
    // 4. 提示文字 "请刷卡"（3个汉字，宽48），居中放置: (128 - 48) / 2 = 40
    LCD_DrawGBKString16x16(40, 73, Card_please_GBK, RGB565_WARNING_YELLOW, WHITE);
    uint8_t uid[4] = {0};
    uint8_t atqa[2] = {0};
    RC522_Status status;
    RC522_Status old_status;
    while (1)
    {
        // 支持按键 11（返回键）非阻塞退出
        if (key_flag == 1)
        {
            key_flag = 0;
            if (keyPress_value == 11)
            {
                keyPress_value = KEY_NONE;
                return;
            }
        }
        // 等待并读取卡片 UID
        status = RC522_ReadUid4(uid, atqa);
        if (old_status != status)
        {
            if (status == RC522_OK)
            {
                uint32_t result = ((uint32_t)uid[0] << 24U) | ((uint32_t)uid[1] << 16U) |
                                  ((uint32_t)uid[2] << 8U) | (uint32_t)uid[3];
                // 遍历比对已录入的卡片
                uint8_t is_verified = 0;
                for (uint8_t i = 0; i < Card_count; i++)
                {
                    if (card_info[i].status == CARD_EXIST && result == card_info[i].card_id)
                    {
                        is_verified = 1;
                        break;
                    }
                }
                if (is_verified)
                {
                    door_status = DOOR_OPEN;
                    Audio_PlayTrack(audio_card_pass);
                    Beep_Flash();
                    // 清除底部反馈区
                    LCD_FillRect(0, 91, 128, 36, WHITE);
                    // 居中显示 "验证通过！"（5个字符，宽80，居中坐标: (128 - 80) / 2 = 24）
                    LCD_DrawGBKString16x16(24, 91, verified_pass_GBK, RGB565_EMERALD_GREEN, WHITE);
                    // 显示 "开锁"（2个字符，宽32）及动态圆点
                    LCD_DrawGBKString16x16(32, 107, open_the_door_GBK, RGB565_EMERALD_GREEN, WHITE);
                    for (uint8_t i = 0; i < 5; i++)
                    {
                        LCD_DrawString8x16(64 + i * 8, 107, ".", RGB565_EMERALD_GREEN, WHITE);
                        Delay_ms(240);
                    }
                    audio_voice_status = AD_ON; // 打开“开关锁”状态提示语音
                    return;
                }
                else
                {
                    door_status = DOOR_CLOSE;
                    Audio_PlayTrack(audio_card_fail);
                    // 清除底部反馈区并居中显示 "验证失败！"
                    LCD_FillRect(0, 91, 128, 36, WHITE);
                    LCD_DrawGBKString16x16(24, 98, verified_failed_GBK, RGB565_CORAL_RED, WHITE);
                    Delay_ms(1000);
                    // 延迟后清除提示，恢复等待刷卡状态
                    LCD_FillRect(0, 91, 128, 36, WHITE);
                }
            }
            else if (status == RC522_TIMEOUT)
            {
                // 卡片移开感应区，正常待机
            }
            else
            {
                // 读卡通信异常等
            }
            old_status = status;
            Delay_ms(20);
        }
    }
}

void Bluetooth_Interface(void)
{
    // 1. 全屏白色背景（与密码、指纹、NFC解锁界面风格保持一致）
    LCD_FillColor(WHITE);

    // 2. 顶部标题栏（深石墨灰底色 + 香槟金字，呼应统一风格）
    LCD_FillRect(0, 0, 128, 20, RGB565_DARK_SLATE);
    // 居中显示 "蓝牙解锁"（4个汉字，宽64）：(128 - 64) / 2 = 32
    LCD_DrawGBKString16x16(32, 2, bluetooth_string1_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_DARK_SLATE);
    LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);

    // 3. 中间绘制蓝牙图标（45x45），居中放置: (128 - 45) / 2 ≈ 42
    // LCD_DrawImageRGB565(42, 25, 45, 45, gImage_bluetooth45_45);
    W64Q25_ReadData(PICTURE_BLUETOOTH_45X45, picture_container, 4050);
    LCD_DrawImageRGB565(45, 25, 45, 45, picture_container);

    // 4. 提示 "等待蓝牙连接中"（7个汉字，宽112），居中放置: (128 - 112) / 2 = 8
    LCD_DrawGBKString16x16(8, 74, wait_for_bluetooth_GBK, RGB565_WARNING_YELLOW, WHITE);

    // 5. 底部初始门锁状态反馈（红灯已关锁 / 绿灯已开锁）
    if (door_status == DOOR_CLOSE)
    {
        LCD_FillRect(32, 104, 4, 4, RGB565_CORAL_RED);
        LCD_DrawGBKString16x16(40, 98, closedoorstatus_GBK, RGB565_CORAL_RED, WHITE);
    }
    else
    {
        LCD_FillRect(32, 104, 4, 4, RGB565_EMERALD_GREEN);
        LCD_DrawGBKString16x16(40, 98, opendoorstatus_GBK, RGB565_EMERALD_GREEN, WHITE);
    }

    while (1)
    {
        switch (JDY31_analyseCmd())
        {
        case BT_CMD_CONNECT:
            // 局部清空提示行，居中显示 "蓝牙已连接！"（6个字符，宽96）: (128 - 96) / 2 = 16
            LCD_FillRect(0, 74, 128, 16, WHITE);
            LCD_DrawGBKString16x16(16, 74, bluetooth_connected_GBK, RGB565_TECH_BLUE, WHITE);
            Audio_PlayTrack(audio_bluetooth_connected);
            break;

        case UNLOCK: // 接收到开锁指令
            // 刷新底部门锁状态为已开锁（绿指示灯 + 翡翠绿字）
            LCD_FillRect(0, 96, 128, 22, WHITE);
            LCD_FillRect(32, 104, 4, 4, RGB565_EMERALD_GREEN);
            LCD_DrawGBKString16x16(40, 98, opendoorstatus_GBK, RGB565_EMERALD_GREEN, WHITE);
            UART_JDY31_SendBuffer("STATE:UNLOCKED\r\n", 16);
            door_status = DOOR_OPEN;
            Beep_Flash();
            audio_voice_status = AD_ON; // 打开“开关锁”状态提示语音
            break;

        case LOCK:
            // 刷新底部门锁状态为已关锁（红指示灯 + 珊瑚红字）
            LCD_FillRect(0, 96, 128, 22, WHITE);
            LCD_FillRect(32, 104, 4, 4, RGB565_CORAL_RED);
            LCD_DrawGBKString16x16(40, 98, closedoorstatus_GBK, RGB565_CORAL_RED, WHITE);
            UART_JDY31_SendBuffer("STATE:LOCKED\r\n", 14);
            door_status = DOOR_CLOSE;
            Beep_Flash();
            audio_voice_status = AD_ON; // 打开“关锁”状态提示语音
            break;

        case exitToMemu:
            return;

        case STATUS:
            if (door_status == DOOR_OPEN)
            {
                UART_JDY31_SendBuffer("STATE:UNLOCKED\r\n", 16); // 注意：\r\n占两个字符！！
            }
            else if (door_status == DOOR_CLOSE)
            {
                UART_JDY31_SendBuffer("STATE:LOCKED\r\n", 14);
            }
            else
            {
                UART_JDY31_SendBuffer("ERR:UNKNOWN_STATE\r\n", 19);
            }
            break;

        default:
            break;
        }

        /**根据标志位，来控制舵机  */
        if ((door_status == DOOR_CLOSE) && (audio_voice_status == AD_ON))
        {
            CLOSE_DOOR;
            Audio_PlayTrack(audio_close_door);
            audio_voice_status = AD_OFF;
        }
        else if ((door_status == DOOR_OPEN) && (audio_voice_status == AD_ON))
        {
            OPEN_DOOR;
            Audio_PlayTrack(audio_open_door);
            audio_voice_status = AD_OFF;
        }
    }
}

void Menu_Interface(void)
{
    // 1. 全屏黑色底色
    LCD_FillColor(RGB565_BLACK);
    // 2. 顶部标题栏（午夜深蓝底色 + 香槟金字）
    LCD_FillRect(0, 0, 128, 20, RGB565_MIDNIGHT_BLUE);
    LCD_DrawGBKString16x16(40, 2, MenuString_GBK, RGB565_CHAMPAGNE_GOLD, RGB565_MIDNIGHT_BLUE);
    LCD_DrawLine(0, 21, 127, 21, RGB565_DARK_GRAY);
    // 3. 中间 5 个选项（均匀分布，行高 16，行间距 1）
    LCD_DrawGBKString16x16(0, 23, guanliyuan_string_GBK, RGB565_WHITE, RGB565_BLACK);
    LCD_DrawGBKString16x16(0, 40, password_string_GBK, RGB565_WHITE, RGB565_BLACK);
    LCD_DrawGBKString16x16(0, 57, fingerprint_string_GBK, RGB565_WHITE, RGB565_BLACK);
    LCD_DrawGBKString16x16(0, 74, Card_string_GBK, RGB565_WHITE, RGB565_BLACK);
    LCD_DrawGBKString16x16(0, 91, bluetooth_string_GBK, RGB565_WHITE, RGB565_BLACK);
    // 4. 底部状态栏分割线
    LCD_DrawLine(0, 108, 127, 108, RGB565_DARK_GRAY);
    // 5. 门锁状态反馈（绿开 / 红关，带指示灯点）
    if (door_status == DOOR_CLOSE)
    {
        LCD_FillRect(32, 116, 4, 4, RGB565_CORAL_RED); // 红指示灯
        LCD_DrawGBKString16x16(40, 110, closedoorstatus_GBK, RGB565_CORAL_RED, RGB565_BLACK);
        CLOSE_DOOR;
    }
    else if (door_status == DOOR_OPEN)
    {
        LCD_FillRect(32, 116, 4, 4, RGB565_EMERALD_GREEN); // 绿指示灯
        LCD_DrawGBKString16x16(40, 110, opendoorstatus_GBK, RGB565_EMERALD_GREEN, RGB565_BLACK);
        OPEN_DOOR;
    }
}

/**
 * @brief 将内存中的所有卡片信息保存到 W25Q64 Flash 中
 * @note 内部将魔数和 card_info 数组打包，通过 W64Q25_SectorProgram 一次性擦写到 0x030000 扇区
 */
void Card_SaveToFlash(void)
{
    Card_Flash_Data flash_data;
    flash_data.magic = CARD_STORAGE_MAGIC;
    // 拷贝当前内存中的所有卡片数据
    for (uint8_t i = 0; i < Card_count; i++)
    {
        flash_data.cards[i] = card_info[i];
    }
    // 将包含魔数和卡片信息的结构体一次性写入 Flash（带扇区自动擦除和重写）
    W64Q25_SectorProgram(CARD_STORAGE_ADDR, (const uint8_t *)&flash_data, sizeof(Card_Flash_Data));
}

/**
 * @brief 开机时从 W25Q64 Flash 加载卡片信息到内存
 * @note 若读出的魔数匹配 CARD_STORAGE_MAGIC，说明之前保存过卡片，直接恢复到 card_info 数组；
 *       若魔数不匹配（芯片首次使用或扇区全为 0xFF），则自动初始化为 6 张空卡并存入 Flash。
 */
void Card_LoadFromFlash(void)
{
    Card_Flash_Data flash_data;
    // 从 Flash 中读取存储结构体
    W64Q25_ReadData(CARD_STORAGE_ADDR, (uint8_t *)&flash_data, sizeof(Card_Flash_Data));

    if (flash_data.magic == CARD_STORAGE_MAGIC)
    {
        // 曾保存过卡片数据，恢复到内存数组
        for (uint8_t i = 0; i < Card_count; i++)
        {
            card_info[i] = flash_data.cards[i];
            card_status[i] = card_info[i].status;
        }
    }
    else
    {
        // 首次上电使用或芯片已被擦除，初始化所有卡片为未录入状态
        for (uint8_t i = 0; i < Card_count; i++)
        {
            card_info[i].status = CARD_EMPTY;
            card_info[i].card_id = 0;
            card_status[i] = CARD_EMPTY;
        }
        // 写入初始状态及魔数，防止下次误判
        Card_SaveToFlash();
    }
}
