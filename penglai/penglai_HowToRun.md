# 在openEuler RISC-V 24.03 on QEMU上运行Penglai PMP
[penglai-enclave-pmp仓库](https://github.com/Penglai-Enclave/Penglai-Enclave-sPMP.git)

从openEuler 24.03 RISC-V开始，penglai PMP相关软件包已经合入openEuler官方软件仓了但是目前还有些问题(因为构建和内核版本有关)，目前仍然需要，通过源码构建。
[Penglai的官方文档足够详细](https://github.com/Penglai-Enclave/Penglai-Enclave-sPMP/blob/opensbi/README.md)，仅有部分细节需要补充，可以[同时参考该文章](https://www.cnblogs.com/world-explorer/p/18064229)
# 注意事项
- `openEuler RISC-V环境`：目前官方提供了支持penglai的uboot(添加了penglai Secure Monitor的uboot)和启动脚本，可以直接拉取用于启动。
- `编译构建`：因为openEuler已经基本集成了penglai PMP，uboot/openSBI和openEuler发行版不用构建
    - `内核模块构建`：kernel module的构建因为依赖openEuler的源码，建议直接在运行penglai PMP的openEuler环境上拉取kernel源码进行编译。
    - `Penglai SDK构建`：为了加速构建，建议在主机环境上通过交叉编译完成构建后直接拷贝`sdk/demo`到openEuler RISC-V中(为了方便理解penglai的工作模式，建议打开`PENGLAI_DEBUG`宏启用`dprint`)。
- `运行`：完成构建后，openEuler RISC-V上已经有penglai demo和kernel module了，插入penglai.ko之后可以测试运行penglai demo，如果开启了`PENGLAI_DEBUG`宏，可以看到更详细的信息。