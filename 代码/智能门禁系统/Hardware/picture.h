#ifndef PICTURE_H
#define PICTURE_H

#include "stm32f10x.h"

// #define PICTURE_EN 1

#ifdef PICTURE_EN

// 很占flash空间，所以在写进flash之后会注释掉，要用的时候再在.h和.c里面解除注释

extern const unsigned char gImage_administrator45_45[];
extern const unsigned char gImage_bluetooth45_45[];
extern const unsigned char gImage_password45_45[];
extern const unsigned char gImage_card45_45[];
extern const unsigned char gImage_fingerprint45_45[];

#endif

typedef struct
{
    uint8_t gbk[2];     /* gbk[0]是第1字节，gbk[1]是第2字节 */
    uint8_t bitmap[32]; /* 取模软件生成的16×16字模 */
} LCD_GBK16Glyph;

#define GBK16_FONT_COUNT 101U

extern const uint8_t font8x16[95][16];
extern const LCD_GBK16Glyph g_gbk16_font[];

extern const uint8_t nihao_GBK[];

/*主菜单界面字库 */
extern const uint8_t MenuString_GBK[];         // 主菜单
extern const uint8_t guanliyuan_string_GBK[];  // 一、管理员
extern const uint8_t password_string_GBK[];    // 二、密码解锁
extern const uint8_t fingerprint_string_GBK[]; // 三、指纹解锁
extern const uint8_t Card_string_GBK[];        // 四、刷卡解锁
extern const uint8_t bluetooth_string_GBK[];   // 五、蓝牙
extern const uint8_t opendoorstatus_GBK[];     // 开门
extern const uint8_t closedoorstatus_GBK[];    // 关门

/*管理员界面字库 */
extern const uint8_t password_exam_GBK[];      // 请输入当前密码
extern const uint8_t password_right_GBK[];     // 密码正确
extern const uint8_t enter_adminisrator_GBK[]; // 进入管理员界面
extern const uint8_t password_error_GBK[];     // 密码错误！
extern const uint8_t try_again_GBK[];          // 请重试

extern const uint8_t administrator_GBK[];      // 管理员
extern const uint8_t changePassword_GBK[];     // 一、修改密码
extern const uint8_t fingerprint_manage_GBK[]; // 二、指纹管理
extern const uint8_t Card_manage_GBK[];        // 三、刷卡管理
extern const uint8_t bluetooth_manage_GBK[];   // 四、蓝牙
extern const uint8_t num_GBK[];                // 号

/*管理员界面 */
/*密码修改界面 */
extern const uint8_t password_string1_GBK[];    // 密码解锁（标题）
extern const uint8_t password_exam1_GBK[];      // 请输入密码
extern const uint8_t enter_new_passsword_GBK[]; // 输入新密码：
/*指纹管理界面 */
extern const uint8_t fingerprint_manage1_GBK[];    // 指纹管理
extern const uint8_t enroll_fingerprint_one_GBK[]; // 一、录入指纹
extern const uint8_t delete_fingerprint_two_GBK[]; // 二、删除指纹
extern const uint8_t enroll_fingerprintNum_GBK[];  // 选择录入的指纹号
extern const uint8_t empty_GBK[];                  // 空
extern const uint8_t delete_allcard_GBK[];         // 全部删除
extern const uint8_t delete_fingerprint_GBK[];     // 删除指纹
extern const uint8_t delete_fingerprintNum_GBK[];  // 选择删除的指纹号
extern const uint8_t delete_secceed_GBK[];         // 删除成功
extern const uint8_t already_recorded_GBK[];       // 已录入
extern const uint8_t enrolling_fingerprint_GBK[];  // 正在录入
extern const uint8_t press_finger_GBK[];           // 请按下指纹
extern const uint8_t press_again_GBK[];            // 请松开后再按下
extern const uint8_t enroll_fingerprint_GBK[];     // 录入指纹
extern const uint8_t enroll_succeed_GBK[];         // 成功录入指纹
extern const uint8_t enroll_error_GBK[];           // 录入错误！
extern const uint8_t please_press_again_GBK[];     // 已松开，请再按下

/*卡片管理*/
extern const uint8_t card_manage1_GBK[];      // 卡片管理
extern const uint8_t enroll_card_title_GBK[]; // 录入卡片
extern const uint8_t delete_card_GBK[];       // 删除卡片
extern const uint8_t enroll_card_one_GBK[];   // 一、录入卡片
extern const uint8_t delete_card_two_GBK[];   // 二、删除卡片
extern const uint8_t enroll_cardNum_GBK[];    // 选择录入的卡片号
extern const uint8_t delete_cardNum_GBK[];    // 选择删除的卡片号
extern const uint8_t enroll_card_GBK[];       // 请录入卡片
extern const uint8_t enroll_succeed1_GBK[];   // 录入成功！

/*密码解锁界面*/
extern const uint8_t changePassword1_GBK[]; // 修改密码

/**指纹解锁界面 */
extern const uint8_t fingerprint1_string_GBK[]; // 指纹解锁
extern const uint8_t verified_pass_GBK[];       // 验证通过！
extern const uint8_t open_the_door_GBK[];       // 开锁
extern const uint8_t open_door_error_GBK[];     // 开锁错误！

/*刷卡解锁界面*/
extern const uint8_t Card_string1_GBK[];    // 刷卡解锁
extern const uint8_t Card_please_GBK[];     // 请刷卡
extern const uint8_t verified_failed_GBK[]; // 验证失败

/*蓝牙解锁界面*/
extern const uint8_t bluetooth_string1_GBK[];   // 蓝牙解锁
extern const uint8_t wait_for_bluetooth_GBK[];  // 等待蓝牙连接中
extern const uint8_t bluetooth_connected_GBK[]; // 蓝牙已连接！

/*另外加的*/
extern const uint8_t change_admin_password_one_GBK[];    /* 一、改管理员密码 */
extern const uint8_t change_unlock_password_two_GBK[];   /* 二、修改开锁密码 */
extern const uint8_t change_admin_password_title_GBK[];  /* 修改管理员密码 */
extern const uint8_t change_unlock_password_title_GBK[]; /* 修改开锁密码 */
extern const uint8_t sound_manage_four_GBK[];            /* 四、声音管理 */
extern const uint8_t sound_manage_title_GBK[];           /* 声音管理 */
extern const uint8_t key_beep_on_one_GBK[];              /* 一、按键音响 */
extern const uint8_t key_beep_off_one_GBK[];             /* 一、按键音停 */
extern const uint8_t prompt_volume_two_GBK[];            /* 二、提示音量 */
extern const uint8_t prompt_volume_title_GBK[];          /* 提示音量设置 */
extern const uint8_t mute_GBK[];                         /* 静音 */
extern const uint8_t enter_admin_password_GBK[];         // 输入管理员密码
extern const uint8_t enter_new_password_no_colon_GBK[];  // 输入新密码（无冒号）
extern const uint8_t increase_volume_GBK[];              // 增高音量
extern const uint8_t decrease_volume_GBK[];              // 降低音量
extern const uint8_t volume_label_GBK[];                 // 音量
extern const uint8_t return_GBK[];                       // 返回

#endif
