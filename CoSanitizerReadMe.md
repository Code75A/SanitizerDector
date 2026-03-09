# CoSanitizer - Artifact                    v1.0

### 0.Prerequisites

We recommend to run the full evaluation on a machine with at least 32 cores, 32GB memory, and 200GB disk space for a reasonable evaluation time (< 5 hours).



### 1. Prepare the environment

we provide the docker file tar for download: link 

download the artifact from the link, then untar the artifact

```
tar-xvf artifact_XXXX2026_CoSanitizer.tar.gz
```

Then, enter the docker container

```
cd /path/to/the/artifact/
```

run `env.sh`

```
./env.sh
```





### 2.1 Example Program

For example, the following is a valid seed program:

```
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

unsigned long shiftone(char v){
    return (int)1 << v;
}

int main(){
    unsigned char a ;
    if (a-1){
        return 0;
    }
    const unsigned long v1 = shiftone(46);
    return 0;
}
```



```
cd sseq/script
./codeTran.sh
```

then you can see example program `example1.c` be instrumented

```
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

int example1_shiftone_Catcher_0=0;
unsigned long shiftone(char v){
	int SaniTestArr[32];
	example1_shiftone_Catcher_0 = *(SaniTestArr + (int)v);
    return (int)1 << v;
}

int example1_main_Catcher_1=1;
int main(){
    int* example1_main_a_copy_unsigned_char=NULL;
    unsigned char a ;
    example1_main_Catcher_1= *example1_main_a_copy_unsigned_char;
    if (a-1){
        return 0;
    }

    const unsigned long v1 = shiftone(46);
    return 0;
}
```



### 2.2 Run CoSanitizer on a given seed program

#### 2.2.0  Structure of CoSanitizer

```
├── braces.cfg     #config file for codeFormatter
├── build          #cmake dir
│   ├── CMakeCache.txt
│   ├── CMakeFiles
│   ├── cmake_install.cmake
│   ├── Makefile
│   └── sseq
├── build.sh         #compiler script
├── CMakeLists.txt   #CMakeList
├── compile_commands #target program compiler_commands
│   ├── example1
|   |   ├── compiler_commands.json
├── scripts          #scripts dir
│   ├── build
│   ├── codeFormatter.sh  #this scripts is use to regulate target program
│   ├── codeTrans.sh      #this scripts is
│   ├── config.sh
│   ├── include_dirs.txt
│   ├── place_in_root
│   ├── res.txt
│   ├── result.txt
│   └── sidescript
├── src                 #the source code for CoSanitizer
│   ├── action.cpp
│   ├── action.h
│   ├── main.cpp
│   ├── seq_info.h
│   ├── tool.cpp
│   └── tool.h
├── support            #some support tool
│   ├── csmith
│   └── llvm-project-llvmorg-19.1.3
```



#### 2.2.1 Get target program compiler_commands

We recommend use `bear --`  to generate the compiler_commands

install tool `bear`

```
sudo apt install bear
```

 then transfer into target project path

```
cd /target/project/path
```

use bear command to generate `compiler_commands.json` at current path

```
bear -- make (Makefile for example)
```

then you can see the `compiler_commands.json`  , copy it to our tool path `sseq/build/projectName`

```
cp compiler_commands.json /root/sseq/build/projectName/
```



#### 2.2.2 Prepare the settings

tool path

```
cd /root/sseq
```

build the project root path

```
./build.sh
```

script

```
cd script
```



modify the `/sseq/scripts/config.sh`settings

```
MutantHome=/root/sseq/example   #modify here as the target project root dictionary
```

modify the `/sseq/scripts/codeTran.sh` settings

```
testOneProgram() {
    programName=$1
    programName=`readlink -f $programName`
    programDir=`dirname $programName`
    cd $programDir

    #run sseq
    echo "getCompileCommand $programName $programDir > $programDir/compile_commands.json"

    #genCompileCommand $programName $programDir > $programDir/compile_commands.json
    cp /root/sseq/compile_commands/example1/compile_commands.json "$programDir/compile_commands.json"    #here
```



```
change the "/root/sseq/compile_commands/example1/compile_commands.json" as "/target_project/compile_commands/compile_commands.json"
```



modify the `/sseq/scripts/codeFormatter.sh ` settings

```
#TARGET_DIR="/path/to/your/source/folder"  # ←←← 请在此处填写你的目标文件夹路径
TARGET_DIR="/root/example
```



#### 2.2.3 Run Our Tool 

run the tool

```
./codeFormatter.sh
./codetrans.sh
```

then you can see the instrumented file in project

such as

```
main.c
main_null.c
```

copy the `main_null.c` source code into` main.c`

compiler the original project

```
make
```

#### 2.2.4 Fuzz and find bugs

Using fuzz to find potential bugs: we recommend AFL++ to do it

