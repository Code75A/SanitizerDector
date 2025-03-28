要编译和运行测试代码，首先使用以下命令选择启用不同的Sanitizer工具进行编译：

使用ASAN（AddressSanitizer）检测内存错误：
make asan

使用UBSAN（UndefinedBehaviorSanitizer）检测未定义行为：
make ubsan

使用MSAN（MemorySanitizer）检测未初始化内存的使用：
make msan

编译完成后，运行以下命令执行测试：
./build/bit_shift_main 001888

运行完成后，可以使用以下命令清理生成的文件：
make clean

通过以上步骤，您可以分别使用不同的Sanitizer工具来编译和运行测试代码，以便检测不同类型的错误



case1来自https://samate.nist.gov/SARD/test-cases/199231/versions/1.0.0
case2来自https://samate.nist.gov/SARD/test-cases/199232/versions/1.0.0