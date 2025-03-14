BASEDIR=$(dirname "$0")
source $BASEDIR/config.sh --source-only


#FP-1 because of wrong of ubfuzz
#to confirm the divisor is zero
fp1CheckOneProgram(){
    programName=$1
    programName=`readlink -f $programName`
    programDir=`dirname $programName`

    echo $programDir

    cd $programDir

    exeProgName=$( basename $programName .c )
    exeProgName=$exeProgName"_fp1.c"
    divProgName=$( basename $programName .c )
    divProgName=$divProgName"_div.c"

    sed -E 's/SaniCatcher[0-9]+=SaniTestArr[0-9]+\[(.+)\];/printf\("output:%dend ",\1\);/' "$divProgName" > "$exeProgName"

    CC=$CLANG
    echo $CC -g -O0 -Wno-everything -I$CSMITH_PATH $exeProgName
    $CC -g -O0 -Wno-everything -I$CSMITH_PATH $exeProgName &> /dev/null
    output=$(./a.out 2>&1)
    echo $output
}
fp1Check(){
    #check for clang 0 vs * or gcc 0 vs *

    echo " " >> $FilterFPOUTPUTFILE
    echo "======filter fp1=====" >> $FilterFPOUTPUTFILE

    #!/bin/bash

    # 输入文件路径
    input_file=$FilterFPOUTPUTFILE

    # 确保输入文件存在
    if [[ ! -f "$input_file" ]]; then
        echo "Error: File not found!"
        exit 1
    fi

    # 处理每一行
    while IFS= read -r line; do
        # 匹配"found a bug"行并提取数据
        # echo $line

        if [[ "$line" =~ filter ]]; then
            break
        fi
        if [[ "$line" =~ found\ a\ bug\ in\ (/.+\.c),\ clang:\ ([0-9]+)\ vs\ ([0-9]+),\ gcc:\ ([0-9]+)\ vs\ ([0-9]+) ]]; then
            # 提取文件路径
            file_path="${BASH_REMATCH[1]}"
            clang_1="${BASH_REMATCH[2]}"
            clang_2="${BASH_REMATCH[3]}"
            gcc_1="${BASH_REMATCH[4]}"
            gcc_2="${BASH_REMATCH[5]}"
            
            # # 输出提取的内容
            # echo "File: $file_path"
            # echo "Clang: $clang_1 vs $clang_2"
            # echo "GCC: $gcc_1 vs $gcc_2"
            # echo "--------------------------"

            # 判断是否 clang_1 或 gcc_1 为 0
            if [[ "$clang_1" -eq 0 || "$gcc_1" -eq 0 ]]; then
                # 输出提取的内容
                # echo "File: $file_path"
                # echo "Clang: $clang_1 vs $clang_2"
                # echo "GCC: $gcc_1 vs $gcc_2"
                # echo "--------------------------"
                fp1CheckOneProgram $file_path
                #if [[ "$output" =~ output:-1end ]]; then
                if [[ "$output" =~ ^[^o]*output:-1end ]]; then
                    continue
                fi
                if [[ "$output" =~ output ]]; then
                    grep -v "$line" "$input_file" > temp.txt && mv temp.txt "$input_file"
                    echo $line" "$output >> $FilterFPOUTPUTFILE
                fi
                

            fi
        fi
    done < "$input_file"

}

if [ "${1}" != "--source-only"  ]; then
    fp1Check
    #fp1CheckOneProgram /home/sd/SanitizerDector/mutants/mutants241210/mutated_0_tmp72elu2jf.c
fi