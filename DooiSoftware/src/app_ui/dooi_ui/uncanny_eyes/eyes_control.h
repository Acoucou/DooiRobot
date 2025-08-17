#ifndef EYES_H
#define EYES_H

#include <stdint.h>
#include <stdbool.h>

// Screen dimensions
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

typedef enum{
    EYES_ANIME,
    EYES_BIGBLUE,
    EYES_SKULL,
    EYES_COUNT,
}EYES_DEFINE;

// Image struct
typedef struct {
    const uint16_t *data;
    uint16_t width;
    uint16_t height;
} Image;

// OverallState struct
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
    uint32_t timeToNextBlinkMs;
    uint32_t timeOfLastBlinkMs;
    uint16_t irisFrame;
    float pupilAmount;
    bool resizing;
    float resizeStart;
    float resizeTarget;
    uint32_t resizeStartTimeMs;
    uint32_t resizeDurationMs;
    int fixate;
} OverallState;

// BlinkState enum
typedef enum {
    NOT_BLINKING,
    BLINK_CLOSING,
    BLINK_OPENING
} BlinkState;

// EyeBlink struct
typedef struct {
    BlinkState state;
    uint32_t durationMs;
    uint32_t startTimeMs;
    float blinkFactor;
} EyeBlink;

// PupilParams struct
typedef struct {
    uint16_t color;
    uint16_t slitRadius;
    float min;
    float max;
} PupilParams;

// IrisParams struct
typedef struct {
    uint16_t radius;
    Image texture;
    uint16_t color;
    uint16_t startAngle;
    float spin;
    uint16_t iSpin;
    uint16_t mirror;
} IrisParams;

// ScleraParams struct
typedef struct {
    Image texture;
    uint16_t color;
    uint16_t startAngle;
    float spin;
    uint16_t iSpin;
    uint16_t mirror;
} ScleraParams;

// EyelidParams struct
typedef struct {
    const uint8_t *upper;
    const uint8_t *lower;
    uint16_t color;
} EyelidParams;

// PolarParams struct
typedef struct {
    uint16_t mapRadius;
    const uint8_t *angle;
    const uint8_t *distance;
} PolarParams;

// EyeDefinition struct
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

// Eye struct
typedef struct {
    const EyeDefinition *definition;
    float x;
    float y;
    uint16_t currentIrisAngle;
    uint16_t currentScleraAngle;
    EyeBlink blink;
    float upperLidFactor;
    float lowerLidFactor;
    bool drawAll;
} Eye;

// EyeController struct
typedef struct {
    Eye eyes[2];
    uint32_t eyeIndex;
    OverallState state;
    bool autoMove;
    bool autoBlink;
    bool autoPupils;
    uint32_t maxGazeMs;     // 最大注视时间
    int32_t nufix;
    float irisPrev[7];
    float irisNext[7];
} EyeController;

extern EyeController control;

// Function declarations
void eye_controller_init(EyeController *controller, const EyeDefinition *def_left, const EyeDefinition *def_right);

void eye_controller_blink(EyeController *controller, uint32_t duration);
void eye_controller_wink(EyeController *controller, uint32_t index);

void eye_controller_set_target_position(EyeController *controller, float xTarget, float yTarget, int32_t durationMs);
void eye_controller_set_position(EyeController *controller, float x, float y);

void eye_controller_set_target_pupil(EyeController *controller, float ratio, int32_t durationMs);
void eye_controller_set_pupil(EyeController *controller, float ratio);

bool eye_controller_render_frame(EyeController *controller);


#endif
