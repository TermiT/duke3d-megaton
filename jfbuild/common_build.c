//
//  common_build.c
//  sw
//
//  Created by Gennadiy Potapov on 23/7/13.
//  Copyright (c) 2013 General Arcade. All rights reserved.
//

#include "common_build.h"
#include "cache1d.h"


int32_t check_file_exist(const char *fn)
{
    int32_t opsm = pathsearchmode;
    char *tfn;
    
    pathsearchmode = 1;
    if (findfrompath(fn,&tfn) < 0)
    {
        char buf[BMAX_PATH];
        
        Bstrcpy(buf,fn);
        kzfindfilestart(buf);
        if (!kzfindfile(buf))
        {
            initprintf("Error: file \"%s\" does not exist\n",fn);
            pathsearchmode = opsm;
            return 1;
        }
    }
    else Bfree(tfn);
    pathsearchmode = opsm;
    
    return 0;
}
