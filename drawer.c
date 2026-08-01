/*
    drawer.h/drawer.c   vram buffer
*/

#include "drawer.h"
#include "display.h"

void draw_exact_cb(boxed_buffer_t *self, void *ctx) {
    // if (self->ctx == 2)
    // LOG_DEBUG("%d->draw_cb(), self->super->first_child = %d, self->super->first_child->next_sibling = %d\n",
    //      self->ctx,self->super->first_child->ctx, self->super->first_child->next_sibling);

    (void)ctx; // 未使用上下文
    
    if (self->w <= 0 || self->h <= 0) return;

    // for (int y = 0; y < self->h; y++) {
    //     for (int x = 0; x < self->w; x++) {
    //         self->framebuffer[y * self->stride + x] = COLOR_RED;
    //     }
    // }

    for (int x = 0; x < self->w; x++) {
        self->framebuffer[0 * self->stride + x] = COLOR_RED;
        self->framebuffer[(self->h - 1) * self->stride + x] = COLOR_RED;
    }

    for (int y = 0; y < self->h; y++) {
        self->framebuffer[y * self->stride + 0] = COLOR_RED;
        self->framebuffer[y * self->stride + self->w - 1] = COLOR_RED;
    }
}


void draw_fuzzy_cb(boxed_buffer_t *self, void *ctx) {
    (void)ctx;

    if (self->w <= 0 || self->h <= 0) return;
    
    for (int y = 0; y < self->h; y++) {
        for (int x = 0; x < self->w; x++) {

            if (x == 0 || x == self->w - 1 || y == 0 || y == self->h - 1) {
                self->framebuffer[y * self->stride + x] = COLOR_GREEN;
            } else {
                self->framebuffer[y * self->stride + x] = COLOR_DARK_GREEN;
            }
        }
    }
}

boxed_buffer_t* isolate_from_display(Display *disp) {
    if (!disp) return NULL;

    boxed_buffer_t *bf = (boxed_buffer_t *) calloc(1, sizeof(boxed_buffer_t));
    if (!bf) return NULL;

    LOG_DEBUG("New box from Display (%d, %d).\n", disp->width, disp->height);

    bf->super = bf;
    bf->refx = 0;
    bf->refy = 0;
    bf->w = disp->width;
    bf->h = disp->height;

    bf->framebuffer = disp->framebuffer; 
    bf->stride = disp->width;

    bf->is_dirty = false; 
    bf->child_dirty = false;
    bf->first_child = NULL;
    bf->next_sibling = NULL;

    bf->draw_cb = draw_exact_cb;
    bf->ctx = NULL;

    return bf;
}

// Reference X, Reference Y
boxed_buffer_t* new_buffer_exact(boxed_buffer_t *super, uint16_t refx, uint16_t refy, uint16_t w, uint16_t h) {
    if ((refx >= super->w) || (refy >= super->h)) return NULL;
    if ((refx + w > super->w) || (refy + h > super->h)) return NULL;

    boxed_buffer_t *bf = (boxed_buffer_t *) calloc(1, sizeof(boxed_buffer_t));
    if (!bf) return NULL;

    bf->super = super;
    bf->refx = refx;
    bf->refy = refy;
    bf->w = w;
    bf->h = h;

    bf->stride = super->stride;

    // stride应该是super的w
    bf->framebuffer = super->framebuffer + (refy * super->stride + refx);

    bf->draw_cb = draw_exact_cb;
    bf->ctx = NULL;

    return bf;
}

// Fuzzy Tree Building
int new_buffer_fuzzy(boxed_buffer_t *super, 
                     uint16_t x1, uint16_t y1, uint16_t w1, uint16_t h1,
                     uint16_t x2, uint16_t y2, uint16_t w2, uint16_t h2,
                     uint16_t threshold,
                     boxed_buffer_t **out1, boxed_buffer_t **out2) 
{
    if (!super || !out1 || !out2) return -1;


    int x_overlap = !((x1 + w1 + threshold <= x2) || (x2 + w2 + threshold <= x1));
    int y_overlap = !((y1 + h1 + threshold <= y2) || (y2 + h2 + threshold <= y1));

    if (x_overlap && y_overlap) {
        uint16_t min_x = MIN(x1, x2);
        uint16_t min_y = MIN(y1, y2);
        uint16_t max_x = MAX(x1 + w1, x2 + w2);
        uint16_t max_y = MAX(y1 + h1, y2 + h2);
        
        uint16_t parent_w = max_x - min_x;
        uint16_t parent_h = max_y - min_y;

        boxed_buffer_t *fuzzy_parent = new_buffer_exact(super, min_x, min_y, parent_w, parent_h);
        if (!fuzzy_parent) return -1;

        fuzzy_parent->draw_cb = draw_fuzzy_cb;

        *out1 = new_buffer_exact(fuzzy_parent, x1 - min_x, y1 - min_y, w1, h1);
        *out2 = new_buffer_exact(fuzzy_parent, x2 - min_x, y2 - min_y, w2, h2);

        if (!*out1 || !*out2) {
            if (*out1) { free(*out1); *out1 = NULL; }
            if (*out2) { free(*out2); *out2 = NULL; }
            free(fuzzy_parent);
            return -1;
        }
        return 1;
    } else {
        *out1 = new_buffer_exact(super, x1, y1, w1, h1);
        *out2 = new_buffer_exact(super, x2, y2, w2, h2);

        if (!*out1 || !*out2) {
            if (*out1) { free(*out1); *out1 = NULL; }
            if (*out2) { free(*out2); *out2 = NULL; }
            return -1;
        }
        return 0;
    }
}

void mark_buffer_dirty(boxed_buffer_t *bf) {
    if (!bf) return;
    
    bf->is_dirty = true;
    
    // Walk up the super chain, marking each ancestor's child_dirty flag.
    // Stop when we reach the root (super == self) or a node already marked.
    boxed_buffer_t *p = bf->super;
    while (p && !p->child_dirty) {
        p->child_dirty = true;
        if (p->super == p) break; // reached root
        p = p->super;
    }
}

void render_sequence(boxed_buffer_t *node) {
    if (!node) return;

    boxed_buffer_t *render_queue_head = NULL;
    boxed_buffer_t *render_queue_tail = NULL;
    
    build_render_queue(node, &render_queue_head, &render_queue_tail);

    boxed_buffer_t *curr = render_queue_head;
    while (curr) {
        if (curr->is_dirty && curr->draw_cb) {
            LOG_DEBUG("%X->draw_cb().\n", curr);
            curr->draw_cb(curr, curr->ctx);
            curr->is_dirty = false;
        }
        // curr->child_dirty = false; 
        
        curr = curr->next_render;
    }
}

// void link_buffer_node(boxed_buffer_t *parent, boxed_buffer_t *child) {
//     if (!parent || !child) return;
//     child->super = parent;
//     child->next_sibling = parent->first_child;
//     parent->first_child = child;
// }

void link_buffer_node(boxed_buffer_t *parent, boxed_buffer_t *child) {
    if (!parent || !child) return;
    child->super = parent;
    child->next_sibling = NULL;

    LOG_DEBUG("Linking %X to %X, $1.refx = %d, $2.refx = %d.\n", child, parent, child->refx, parent->refx);

    if (!parent->first_child) {
        parent->first_child = child;
    } else {
        boxed_buffer_t *curr = parent->first_child;
        while (curr->next_sibling) {
            curr = curr->next_sibling;
        }
        curr->next_sibling = child;
    }
}

static void build_render_queue(boxed_buffer_t *node, boxed_buffer_t **queue_head, boxed_buffer_t **queue_tail) {
    if (!node) return;

    if (!node->is_dirty && !node->child_dirty) {
        return; 
    }

    if (*queue_tail) {
        (*queue_tail)->next_render = node;
    } else {
        *queue_head = node;
    }
    
    *queue_tail = node;
    node->next_render = NULL;

    boxed_buffer_t *child = node->first_child;
    while (child) {
        build_render_queue(child, queue_head, queue_tail);
        child = child->next_sibling;
    }
}

void free_buffer_tree(boxed_buffer_t *node) {
    if (!node) return;
    boxed_buffer_t *child = node->first_child;
    while (child) {
        boxed_buffer_t *next = child->next_sibling;
        free_buffer_tree(child);
        child = next;
    }
    free(node);
}

// for test
void draw_rounded_rect_cb(boxed_buffer_t *self, void *ctx) {
    if (!self || self->w <= 0 || self->h <= 0) return;

    uint16_t r = 8;
    pixel_t b_color = COLOR_RED;
    pixel_t f_color = COLOR_BLACK;
    bool draw_fill = true;

    if (ctx) {
        rounded_rect_config_t *config = (rounded_rect_config_t *)ctx;
        r = config->radius;
        b_color = config->border_color;
        f_color = config->fill_color;
        draw_fill = config->draw_fill;
    }

    uint16_t max_r = MIN(self->w, self->h) / 2;
    if (r > max_r) r = max_r;

    int32_t r_sq = r * r;
    int32_t inner_r_sq = (r > 0) ? (r - 1) * (r - 1) : 0;

    for (int y = 0; y < self->h; y++) {
        for (int x = 0; x < self->w; x++) {
            
            if (r == 0) {
                if (x == 0 || x == self->w - 1 || y == 0 || y == self->h - 1) {
                    self->framebuffer[y * self->stride + x] = b_color;
                } else if (draw_fill) {
                    self->framebuffer[y * self->stride + x] = f_color;
                }
                continue;
            }

            int32_t dx = 0;
            if (x < r) {
                dx = r - x;
            } else if (x >= self->w - r) {
                dx = x - (self->w - 1 - r);
            }

            int32_t dy = 0;
            if (y < r) {
                dy = r - y;
            } else if (y >= self->h - r) {
                dy = y - (self->h - 1 - r);
            }

            if (dx > 0 && dy > 0) {
                int32_t dist_sq = dx * dx + dy * dy;
                if (dist_sq > r_sq) {
                    continue; 
                } else if (dist_sq > inner_r_sq) {
                    self->framebuffer[y * self->stride + x] = b_color;
                } else if (draw_fill) {
                    self->framebuffer[y * self->stride + x] = f_color;
                }
            } else {
                if (dx == r || dy == r) {
                    self->framebuffer[y * self->stride + x] = b_color;
                } else if (draw_fill) {
                    self->framebuffer[y * self->stride + x] = f_color;
                }
            }
        }
    }
}


void draw_picture_cb(boxed_buffer_t *self, void *ctx) {
    if (!self || !ctx || self->w <= 0 || self->h <= 0) return;
    picture_config_t *cfg = (picture_config_t *)ctx;
    
    for (int y = 0; y < self->h && y < cfg->pic_h; y++) {
        for (int x = 0; x < self->w && x < cfg->pic_w; x++) {
            self->framebuffer[y * self->stride + x] = cfg->picture_data[y * cfg->pic_w + x];
        }
    }
}


void build_smart_render_tree(boxed_buffer_t *root, ui_control_t *controls, int count, int threshold) {
    if (!root || !controls || count <= 0) return;

    int parent[64];
    for (int i = 0; i < count; i++) parent[i] = i;

    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            ui_control_t *a = &controls[i];
            ui_control_t *b = &controls[j];
            
            int no_overlap = (a->x + a->w + threshold <= b->x) || 
                             (b->x + b->w + threshold <= a->x) || 
                             (a->y + a->h + threshold <= b->y) || 
                             (b->y + b->h + threshold <= a->y);
            if (!no_overlap) {
                int root_i = i, root_j = j;
                while (parent[root_i] != root_i) root_i = parent[root_i];
                while (parent[root_j] != root_j) root_j = parent[root_j];
                if (root_i != root_j) parent[root_i] = root_j;
            }
        }
    }

    int min_x[64], min_y[64], max_x[64], max_y[64];
    bool is_group_root[64] = {false};
    int group_count[64] = {0};

    for (int i = 0; i < count; i++) {
        min_x[i] = 10000; min_y[i] = 10000;
        max_x[i] = -10000; max_y[i] = -10000;
    }

    for (int i = 0; i < count; i++) {
        int root_id = i;
        while (parent[root_id] != root_id) root_id = parent[root_id];
        parent[i] = root_id; 
        
        is_group_root[root_id] = true;
        group_count[root_id]++;

        ui_control_t *a = &controls[i];
        if (a->x < min_x[root_id]) min_x[root_id] = a->x;
        if (a->y < min_y[root_id]) min_y[root_id] = a->y;
        if (a->x + a->w > max_x[root_id]) max_x[root_id] = a->x + a->w;
        if (a->y + a->h > max_y[root_id]) max_y[root_id] = a->y + a->h;
    }


    // LOG_DEBUG("control[0] = %X.\n", controls[0]);

    for (int i = 0; i < count; i++) {
        if (!is_group_root[i]) continue;

        int c_min_x = MAX(0, min_x[i]);
        int c_min_y = MAX(0, min_y[i]);
        int c_max_x = MIN(root->w, max_x[i]);
        int c_max_y = MIN(root->h, max_y[i]);

        if (c_min_x >= c_max_x || c_min_y >= c_max_y) continue;

        if (group_count[i] > 1) {
            int p_w = c_max_x - c_min_x;
            int p_h = c_max_y - c_min_y;
            
            boxed_buffer_t *fuzzy_parent = new_buffer_exact(root, c_min_x, c_min_y, p_w, p_h);
            
            if (fuzzy_parent) {
                fuzzy_parent->draw_cb = draw_fuzzy_cb; 
                link_buffer_node(root, fuzzy_parent);
                mark_buffer_dirty(fuzzy_parent);
                
                for (int j = 0; j < count; j++) {
                    if (parent[j] == i) {
                        ui_control_t *ctrl = &controls[j];

                        int child_x = MAX(c_min_x, ctrl->x);
                        int child_y = MAX(c_min_y, ctrl->y);
                        int child_w = MIN(c_max_x, ctrl->x + ctrl->w) - child_x;
                        int child_h = MIN(c_max_y, ctrl->y + ctrl->h) - child_y;

                        if (child_w <= 0 || child_h <= 0) continue;

                        boxed_buffer_t *child = new_buffer_exact(fuzzy_parent, 
                                                                 child_x - c_min_x, 
                                                                 child_y - c_min_y, 
                                                                 child_w, child_h);
                        if (child) {
                            child->draw_cb = ctrl->draw_cb;
                            child->ctx = ctrl->ctx;
                            link_buffer_node(fuzzy_parent, child);
                            mark_buffer_dirty(child);
                        } else {
                            LOG_DEBUG("Fuzzy child failed! x:%d y:%d w:%d h:%d\n", child_x - c_min_x, child_y - c_min_y, child_w, child_h);
                        }
                    }
                }
            }
        } else {
            for (int j = 0; j < count; j++) {
                if (parent[j] == i) {
                    ui_control_t *ctrl = &controls[j];
                    
                    int child_x = MAX(0, ctrl->x);
                    int child_y = MAX(0, ctrl->y);
                    int child_w = MIN(root->w, ctrl->x + ctrl->w) - child_x;
                    int child_h = MIN(root->h, ctrl->y + ctrl->h) - child_y;

                    if (child_w <= 0 || child_h <= 0) break;

                    boxed_buffer_t *child = new_buffer_exact(root, child_x, child_y, child_w, child_h);
                    if (child) {
                        child->draw_cb = ctrl->draw_cb;
                        child->ctx = ctrl->ctx;
                        link_buffer_node(root, child);
                        mark_buffer_dirty(child);
                    } else {
                        LOG_DEBUG("Single child failed! x:%d y:%d w:%d h:%d\n", child_x, child_y, child_w, child_h);
                    }
                    break;
                }
            }
        }
    }
}