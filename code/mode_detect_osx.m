//
//  mode_detect_osx.m
//  duke3d
//
//  Created by serge on 27/2/13.
//  Copyright (c) 2013 General Arcade. All rights reserved.
//

#include <stdio.h>
#import <Cocoa/Cocoa.h>

void dnGetScreenSize(int *width, int *height) {
    NSRect screenRect = [[NSScreen mainScreen] frame];
    *width = (int) screenRect.size.width;
    *height = (int) screenRect.size.height;
}
