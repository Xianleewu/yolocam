#include <stdio.h>

struct Rect {
  int x, y, w, h;
};

// 生成四个边线的矩形区域坐标
void generateRectangles(int startX, int startY, int width, int height,
                        int borderWidth, Rect rectangles[4]) {
  // Top line
  rectangles[0] = {startX, startY, width, borderWidth};
  // Bottom line
  rectangles[1] = {startX, startY + height - borderWidth, width, borderWidth};
  // Left line
  rectangles[2] = {startX, startY, borderWidth, height};
  // Right line
  rectangles[3] = {startX + width - borderWidth, startY, borderWidth, height};
}
