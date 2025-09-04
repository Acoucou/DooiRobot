// eyes_control.h
#ifndef EYES_H
#define EYES_H

#include <stdint.h>
#include <stdbool.h>

// Screen dimensions
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

typedef enum{
    EYES_LID_CLASSIC,
    EYES_LID_OPEN,
    EYES_LID_ANGRY,
    EYES_LID_ANGRY2,
    EYES_LID_SAD,
    EYES_LID_SUSPICIOUS,
    EYES_LID_ANGET_HAPPY,
    EYES_LID_CHEERFUL,
    EYES_LID_COUNT,
}EYES_LIDS_DEFINE;

typedef enum {
    EYES_EMOTION_DEFAULT,
    EYES_EMOTION_FOCUS_BELOW_CENTER,
    EYES_EMOTION_FOCUS_TOP_CENTER,
    EYES_EMOTION_FOCUS_TOP_LEFT,
    EYES_EMOTION_FOCUS_TOP_RIGHT,
    EYES_EMOTION_FOCUS_LEFT,
    EYES_EMOTION_FOCUS_RIGHT,
    EYES_EMOTION_HAPPY,
    EYES_EMOTION_HAPPY1,
    EYES_EMOTION_HAPPY2,
    EYES_EMOTION_ANGRY,
    EYES_EMOTION_ANGRY1,
    EYES_EMOTION_ANGRY_RIGHT,
    EYES_EMOTION_ANGRY_LEFT,
    EYES_EMOTION_SAD, 
    EYES_EMOTION_SUSPICIOUS,
    EYES_EMOTION_AMAZED,
    EYES_EMOTION_COUNT,
}EYES_EMOTION;

typedef enum {
    MAT_BACKGROUND,
    MAT_SCLERA,
    MAT_IRIS,
    MAT_PUPIL
} MaterialType;

// 表情预设
typedef struct {
    EYES_LIDS_DEFINE left_lids;      // 使用的眼睑资源类型
    EYES_LIDS_DEFINE right_lids;
    float left_upper;                // 上眼睑目标开度 [0..1]
    float left_lower;                // 下眼睑目标开度 [0..1]
    float right_upper;
    float right_lower;
    float left_scale;                // 虹膜+瞳孔整体 scale [0.5..2.0]
    float right_scale;

    float pupil;                     // 瞳孔收缩量 [0..1]，小=大瞳孔

    float left_posX;                 // 目标位置 x（-1..1，左负右正）
    float left_posY;                 // 目标位置 y（-1..1，上负下正）
    float right_posX;
    float right_posY;

    uint16_t easePosMs;              // 位置平滑时长
    uint16_t easeScaleMs;            // scale 平滑时长
    uint16_t easePupilMs;            // pupilAmount 平滑时长

    uint8_t displayMode;       // 显示模式（如镜像）  
    bool autoMove;                   // 开关：自动扫视
    bool autoBlink;                  // 开关：自动眨眼
    bool autoPupils;                 // 开关：自动瞳孔
} EyeExpressionPreset;

typedef struct {
    const uint16_t *data;
    uint16_t width;
    uint16_t height;
} Image;

// 每只眼的状态
typedef struct {
    bool inMotion;
    float eyeOldX;
    float eyeOldY;
    float eyeNewX;
    float eyeNewY;
    uint32_t moveStartTimeMs;
    uint32_t moveDurationMs;
    uint32_t lastSaccadeStopMs;
    uint32_t saccadeIntervalMs;

    // 眨眼随机定时
    uint32_t timeToNextBlinkMs;
    uint32_t timeOfLastBlinkMs;

    // 自动 pupilAmount 基础值（独立控制瞳孔大小）
    uint16_t irisFrame;
    float pupilAmount;

    // 瞳孔（pupilAmount）动画（独立通道）
    bool pupilResizing;
    float pupilStart;
    float pupilTarget;
    uint32_t pupilStartTimeMs;
    uint32_t pupilDurationMs;

    // 虹膜+瞳孔整体缩放 scale 动画（独立通道）
    bool scaleResizing;
    float scaleStart;
    float scaleTarget;
    uint32_t scaleStartTimeMs;
    uint32_t scaleDurationMs;

    int fixate;
} OverallState;

typedef enum {
    NOT_BLINKING,
    BLINK_CLOSING,
    BLINK_OPENING
} BlinkState;

typedef struct {
    BlinkState state;
    uint32_t durationMs;
    uint32_t startTimeMs;
    float blinkFactor;
} EyeBlink;

typedef struct {
    uint16_t color;
    uint16_t slitRadius;
    float min;
    float max;
} PupilParams;

typedef struct {
    uint16_t radius;
    Image texture;
    uint16_t color;
    uint16_t startAngle;
    float spin;
    uint16_t iSpin;
    uint16_t mirror;
} IrisParams;

typedef struct {
    Image texture;
    uint16_t color;
    uint16_t startAngle;
    float spin;
    uint16_t iSpin;
    uint16_t mirror;
} ScleraParams;

typedef struct {
    const uint8_t *upper; // 每列两个字节：openY, closeY
    const uint8_t *lower;
    uint16_t color;
} EyelidParams;

typedef struct {
    uint16_t mapRadius;
    const uint8_t *angle;
    const uint8_t *distance;
} PolarParams;

typedef struct {
    char name[16];
    uint16_t radius;
    uint16_t backColor;
    bool tracking;
    float squint;
    const uint8_t *displacement;
    PupilParams pupil;
    IrisParams iris;
    ScleraParams sclera;
    EyelidParams eyelids;
    PolarParams polar;
} EyeDefinition;

typedef struct {
    EyeDefinition *definition;
    float x;
    float y;
    uint16_t currentIrisAngle;
    uint16_t currentScleraAngle;
    EyeBlink blink;

    // 渲染时的平滑开度
    float upperLidFactor;
    float lowerLidFactor;

    // 用户设置的目标开度（每只眼独立）
    float upperLidTarget;
    float lowerLidTarget;

    // 为每只眼睛独立存储当前使用的眼睑资源指针
    const uint8_t *currentUpperLid;
    const uint8_t *currentLowerLid;

    bool drawAll; // 下帧是否全覆盖刷新

    float scale;  // 虹膜+瞳孔整体缩放

    OverallState state;
} Eye;

typedef struct {
    Eye eyes[2];
    uint8_t eyeIndex;
    uint8_t eye_disp_mode;
    uint8_t eye_disp_lastmode;
    uint8_t eye_emotion_type;
    bool autoMove;
    bool autoBlink;
    bool autoPupils;  
    uint16_t maxGazeMs;
    int16_t nufix;
} EyeController;

extern EyeController control;

// 初始化
void eye_controller_init(EyeController *controller, const EyeDefinition *def_left, const EyeDefinition *def_right);

// 眨眼
void eye_controller_blink(EyeController *controller, uint32_t duration); // 双眼
void eye_controller_wink(EyeController *controller, uint32_t index);     // 单眼

// 位置（双眼，同时）
void eye_controller_set_target_position(EyeController *controller, float xTarget, float yTarget, int32_t durationMs);
void eye_controller_set_position(EyeController *controller, float x, float y);

// 位置（单眼）
void eye_controller_set_target_position_eye(EyeController *controller, uint32_t eyeIndex, float xTarget, float yTarget, int32_t durationMs);
void eye_controller_set_position_eye(EyeController *controller, uint32_t eyeIndex, float x, float y);

// pupil（瞳孔独立控制）
void eye_controller_set_target_pupil(EyeController *controller, float ratio, int32_t durationMs);
void eye_controller_set_pupil(EyeController *controller, float ratio);
void eye_controller_set_target_pupil_eye(EyeController *controller, uint32_t eyeIndex, float ratio, int32_t durationMs);
void eye_controller_set_pupil_eye(EyeController *controller, uint32_t eyeIndex, float ratio);

// 渲染一帧（交替渲染左右眼）
bool eye_controller_render_frame(EyeController *controller);

// 虹膜+瞳孔整体缩放（scale）
void eye_controller_set_eye_scale(EyeController *ctl, float scale_left, float scale_right); // 立即设置双眼缩放

// 设置眼睑资源（每只眼独立）
void eye_controller_set_eyelib(uint8_t eye_index, uint8_t eye_lid_type, float upperFactor, float lowerFactor);

void eye_display_frame(EyeController *controller);
void eye_controller_set_display_mode(EyeController *controller, uint8_t mode);

// 虹膜+瞳孔整体 scale
void eye_controller_set_target_scale_eye(EyeController *controller, uint32_t eyeIndex, float ratio, int32_t durationMs);
void eye_controller_set_target_eye_scale(EyeController *controller, float scale_left, float scale_right, int32_t durationMs);

// 应用表情预设/枚举
void eye_controller_apply_preset(EyeController *controller, const EyeExpressionPreset *preset);
void eye_controller_apply_emotion(EyeController *controller, EYES_EMOTION emotion);

#endif