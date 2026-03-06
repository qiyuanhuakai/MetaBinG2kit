```markdown
# MetaBinG2kit

### 该软件包含 MetaBinG2 的源文件以及用于统计和可视化的附加文件。
### This software includes the source file of MetaBinG2 and the additional file designed for statistics and visualization.

请访问 http://cgm.sjtu.edu.cn/MetaBinG2Web/ 下载完整的 MetaBinG2kit 脚本（包括数据库和用例），或仅使用可视化部分。
Please go to http://cgm.sjtu.edu.cn/MetaBinG2Web/ to download the complete scripts of MetaBinG2kit including the database and the use case or to just use the visualization part.

---

## MetaBinG2 运行要求 / Requirements

- 一块 GPU 显卡 / One GPU card
- CUDA 编译器 / CUDA Compiler
- 库：cudart 和 cublas / Libraries: cudart and cublas
- Perl（统计和可视化部分需要）/ perl (required by statistics part and visualization part)

---

## 安装 / Install

```bash
git clone https://github.com/qiyuanhuakai/MetaBinG2kit.git
cd MetaBinG2kit
make
```

*编译 CPU 版本 / Compile for the CPU version:*
```bash
gcc -o MetaBinG2_CPU MetaBinG2_CPU.c cblas_LINUX.a blas_LINUX.a -lm -lpthread -lgfortran -std=c99
```

---

## 分类 / Classify

```bash
./runMetaBinG2 -i [FASTA file] -o [Outfile name] -d [Database]
```

---

## 合并统计结果并生成可视化文件 / Combine the statistic results and generate file for visualization

### (1) 选择要比较的样本 / Select samples to compare

创建 sampleList / Create sampleList:
```
sample1.out.stats
sample2.out.stats
sample3.out.stats
...
```

### (2.1) 生成可视化用的统计文件 / Generate statistic file for visualization

```bash
perl stats.all.pl sampleList all.stats
```

（all.stats 可上传至 MetaBinG2 网站生成可视化部分 / all.stats can be uploaded to MetaBinG2's website to generate the visualization part）

### (2.2) 生成统计文件及可视化部分 / Generate statistic file and the visualization part

```bash
perl stats.all.pl sampleList all.stats p
```

（可在 index.html 中查看可视化部分 / You can check the visualization part in index.html）

---

## 示例 / Example

示例目录中有 3 个样本：SRR3438907.fa、SRR3438908.fa、SRR3438946.fa
We have 3 samples in example directory: SRR3438907.fa, SRR3438908.fa, SRR3438946.fa

### 1. 分类并获取各样本的群落组成结构 / Classify and get community composition structure of each sample

```bash
./runMetaBinG2 -i ./example/SRR3438907.fa -o ./example/SRR3438907.out -d db
./runMetaBinG2 -i ./example/SRR3438908.fa -o ./example/SRR3438908.out -d db
./runMetaBinG2 -i ./example/SRR3438946.fa -o ./example/SRR3438946.out -d db
```

### 2. 选择要比较的样本 / Select samples to compare

创建 sample.list / Create sample.list:
```
./example/SRR3438907.out.stats
./example/SRR3438908.out.stats
./example/SRR3438946.out.stats
```

### 3. 整合与可视化 / Integration and visualization

```bash
perl stats.all.pl ./example/sample.list ./example/all.stats
```
或 / or
```bash
perl stats.all.pl ./example/sample.list ./example/all.stats p
```

- (i) all.stats 可用于 MetaBinG2 网站 / all.stats can be used in the MetaBinG2 website
- (ii) 选择参数 'p' 时，将获得可视化结果，可通过 index.html 查看。请确保 ref 目录（在 MetaBinG2kit 中）与 stats.all.pl 位于同一目录 / When you select the parameter 'p', you will get the visualization result and it can be checked through index.html. Please be sure that the ref dir (in MetaBinG2kit) and the stats.all.pl are in the same directory
```