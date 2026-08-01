
typedef struct animation_object {
    uint32_t start_time;
    uint32_t duration;

    int32_t start_value;
    int32_t end_value;

    struct ui_object *target;

    int32_t (*easing_cb)(int32_t *start, int32_t *end, uint32_t prog);

    void (*exec_cb)(struct ui_object *target, int32_t *current_value);

    struct animation_object *next;
} animation_object_t;


typedef struct widget_object {
    // typedef enum {
    //     WIDGET_PICTURE,
    //     WIDGET_RAW, // dangerous
    //     WIDGET_RECT,
    // } widget_type;

    void *ctx; // for the information will be put in creating;
} widget_object_t;


typedef struct ui_object {
    // only for render and anim computing
    // base xywh will be only copied/computed once at creating
    int16_t base_x, base_y;
    uint16_t base_w, base_h;

    uint16_t x, y;
    uint16_t w, h;

    struct animation_object *anim;

    void *user_data;

    void (*draw_cb)(struct ui_object *self, );
    void (*event_cb)(struct ui_object *self, void *e);

    void (*low_level_painting)(struct ui_object *self, void *ctx); // dangerous

    // Tree view
    struct ui_object *super;
    struct ui_object *next_sibling;
    struct ui_object *first_child;

} ui_object_t;

// will be impl in widgets.h
typedef struct {

    uint16_t base_x, base_y;
    uint16_t base_w, base_h;

    uint16_t radius;

    pixel_t border_color;
    pixel_t fill_color;

    bool draw_fill;

} rounded_rect_config_t2;


typedef struct ui_rect {
    int32_t x, y, w, h;
} ui_rect_t;

static inline bool rect_is_empty(const ui_rect_t *rect) {
    return (rect->w <= 0 || rect->h <= 0);
}

static inline ui_rect_t rect_interset(const ui_rect_t *rect1, const ui_rect_t *rect2) {
    ui_rect_t t;

    t.x = 
}