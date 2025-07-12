#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <font.h>
#include "lv_font_montserrat_48.c"

#define Font lv_font_montserrat_48

int x_clr, y_clr = 0;
int x_write, y_write = 0;

// �����ַ�����ṹ��
typedef struct {
    int top;        // �ϱ߽�
    int bottom;     // �±߽�
    int left;       // ��߽�
    int right;      // �ұ߽�
} area;

// �ַ�����ָ��
size_t size = 0;
area* areas = NULL;

// �������������µ��ַ�����
int add_area(area new_area) {
    // ��չ�����С
    area* new_array = realloc(areas, (size + 1) * sizeof(area));
    if (new_array == NULL) {
        return -1; // �ڴ����ʧ��
    }
    areas = new_array; // ����ָ��

    // ����������
    areas[size] = new_area;
    size++;

    return 0;
}

// ���Ҳ�ɾ����������������
int find_remove_area(int x, int y) {
    for (size_t i = 0; i < size; ++i) {
        if (areas[i].top < x && x < areas[i].bottom && areas[i].left < y && y < areas[i].right) {
            // ����������
            uint8_t pBuf[(areas[i].right - areas[i].left + 1) / 2];
            memset(pBuf, 0, sizeof(pBuf));
            for (int j = 0; j < (areas[i].bottom - areas[i].top); j++) {
                display_image(areas[i].top + j, areas[i].left, pBuf, sizeof(pBuf));
            }

            // ��������ɾ���������
            for (size_t j = i; j < size - 1; ++j) {
                areas[j] = areas[j + 1];
            }
            size--;

            // ���·����ڴ�����С�����С
            areas = realloc(areas, size * sizeof(area));
            if (areas == NULL) {
                return -1;
            }

            return 0; // �ɹ��ҵ���ɾ��
        }
    }

    return -1;
}

// ���һ���ַ�����
int clr_char(void) {
    for (int i = 0; i < 640 * 480; i++) {
        if (find_remove_area(x_clr, y_clr) == 0) {
            return 0;
        }
        else {
            y_clr += 1;
            if (y_clr >= 640) {
                x_clr += 1;
                y_clr = 0;
                if (x_clr >= 480) {
                    x_clr = 0;
                }
            }
        }
    }

    return -1;
}

// ��ӡ area �����е�����ֵ
void print_areas() {
    for (size_t i = 0; i < size; i++) {
        printf("Area %zu: top = %d, bottom = %d, left = %d, right = %d\n",
            i, areas[i].top, areas[i].bottom, areas[i].left, areas[i].right);
    }
}

// ��UTF-8����תΪUnicode��
uint32_t utf8_to_unicode(const char** str) {
    uint32_t unichar = 0;
    if (**str & 0x80) {
        // ���ֽ��ַ�
        if (**str & 0x40) {
            // 3�ֽ��ַ�
            unichar = (**str & 0x0F) << 12;
            (*str)++;
            unichar |= (**str & 0x3F) << 6;
            (*str)++;
            unichar |= (**str & 0x3F);
            (*str)++;
        }
        else {
            // 2�ֽ��ַ�
            unichar = (**str & 0x1F) << 6;
            (*str)++;
            unichar |= (**str & 0x3F);
            (*str)++;
        }
    }
    else {
        // ���ֽ��ַ�
        unichar = **str;
        (*str)++;
    }
    return unichar;
}

// ���unicode�Ƿ����ַ�����
bool check_chars(uint32_t unicode, const char* chars) {
    const char* p = chars;
    while (*p) {
        if (*p == unicode) {
            return true;
        }
        p++;
    }
    return false;
}

// ���ַ���λͼ����д����Ļ
int write_char(int x, int y, const lv_font_t* font, uint32_t c, int adv_x, int adv_y) {
    lv_font_glyph_dsc_t g;
    const uint8_t* bmp = lv_font_get_glyph_bitmap(font, c);     // ��ȡλͼ����

    // ���ض��ַ��Ż�
    const char* chars1 = "abcdefhiklmnorstuvwxz,��.����_";
    const char* chars2 = "gjpqy";
    const char* chars3 = "^\"��'��~";

    if (lv_font_get_glyph_dsc(font, &g, c, '\0')) {
        if (bmp == NULL) {
            fprintf(stderr, "Null Font!\n");
            return -1;
        }
        // ��ȡ�ַ���ʵ�ʿ���
        int width = g.box_w;
        int height = g.box_h;

        // �����к�����ʼ
        int row = x + ((adv_y - height) / 2);
        int col = y + ((adv_x - width) / 2);

        // ���ض��ַ��Ż�
        if (check_chars(c, chars1)) {
            row = x + adv_y * 0.85 - height;
        }
        if (check_chars(c, chars2)) {
            row = x + adv_y - height;
        }
        if (check_chars(c, chars3)) {
            row = x + adv_y * 0.15;
        }

        if (width % 2 == 0) {    // ����Ϊż��������
            uint8_t pBuf[width / 2];
            for (int row_add = 0; row_add < height; row_add++) {
                for (int i = 0; i < width / 2; i++) {
                    pBuf[i] = bmp[row_add * width / 2 + i];
                }
                display_image(row + row_add, col, pBuf, width / 2);
            }
        }
        else {                   // ����Ϊ����������
            uint8_t pBuf[(width + 1) / 2];
            for (int row_add = 0, j = 0; row_add < height; row_add += 2, j += width) {
                for (int i = 0; i < (width + 1) / 2; i++) {
                    pBuf[i] = bmp[j + i];
                }
                pBuf[(width - 1) / 2] &= 0xF0;                              // ĩ��������
                display_image(row + row_add, col, pBuf, (width + 1) / 2);

                //д��һ������
                if (row_add + 1 < height) {
                    uint8_t new_Buf[((width + 1) / 2)+1];
                    uint8_t last_pixel = bmp[j + ((width - 1) / 2)] & 0x0F; // ��¼��ĩ����
                    memset(pBuf, 0, sizeof(pBuf));
                    memset(new_Buf, 0, sizeof(new_Buf));
                    for (int i = 0; i < (width - 1) / 2; i++) {
                        pBuf[i] = bmp[j + ((width + 1) / 2) + i];
                    }
                    // ������Ԫ������4λ
                    for (int k = 0; k < (width - 1) / 2; ++k) {
                        new_Buf[k + 1] = (pBuf[k] << 4) & 0xF0; // ��ǰ��4λ�Ƶ��¸���4λ
                        new_Buf[k] |= pBuf[k] >> 4;             // ��ǰ��4λ��ǰ����4λ�ϲ�
                    }
                    new_Buf[0] |= (last_pixel << 4);            // ����ĩ�����Ƶ���4λ
                    display_image(row + row_add+1, col, new_Buf, (width + 1) / 2);
                }
            }
        }
    }

    return 0;
}

// ��ʾ�ַ���
int display_string(const char* text) {
    const lv_font_t* font = &Font;   // ѡ������
    const char* p = text;
    int adv_y = lv_font_get_line_height(font);                // ��ȡ�ַ��Ľ���߶�
    const char* chars1 = "j";
    // ʹ����Ϊ����
    while (480 % adv_y != 0) {
        adv_y--;
    }

    while (*p) {
        uint32_t unicode = utf8_to_unicode(&p);
        int adv_x = lv_font_get_glyph_width(font, unicode, 0);// ��ȡ�ַ��Ľ������

        // ���ض��ַ��Ż�
        if (check_chars(unicode, chars1)) {
            adv_x *= 1.5;
        }

        // ����ַ�����
        for (int n = 0; n < adv_x + adv_y; n++) {
            find_remove_area(x_write + (adv_y / 2), y_write + n);
        }

        // �����ַ�����
        area new_area1 = { x_write, x_write + adv_y, y_write, y_write + adv_x };
        if (add_area(new_area1) != 0) {
            perror("Failed to add area");
            return -1;
        }

        // �Զ�����
        if (y_write + adv_x >= 640) {
            x_write += adv_y;
            y_write = 0;
            if (x_write + adv_y >= 480) {
                x_write = 0;
            }
        }

        if (write_char(x_write, y_write, font, unicode, adv_x, adv_y) == 0) {
            y_write += adv_x;
            // �Զ�����
            if (y_write + adv_x >= 640) {
                x_write += adv_y;
                y_write = 0;
                if (x_write + adv_y >= 480) {
                    x_write = 0;
                }
            }
        }
    }

    return 0;
}

//ϵͳʱ�Ӻ���
uint32_t custom_tick_get(void) {
    static uint64_t start_ms = 0;
    if (start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}