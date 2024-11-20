# SanitizerDector
modify the build.sh first


secondly, modify config.sh in script folder


then, run `bash scripts/gen_testing.sh`


the results is saved in result.txt file

run `bash scripts/reducefile.sh buggyfile` to reduce the buggy file. For example, `bash scripts/reducefile.sh ../mutants/mutated_0_tmpyzemsynh.c`
