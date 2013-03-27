//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef WINTOOLS_H
#define WINTOOLS_H

int Win32_ShowErrorMessage(const char *caption, const char *message);

void Win32_InitQPC();
double Win32_GetQPC();

#endif /* WINTOOLS_H */
