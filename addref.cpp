#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <cassert>

// --- Configuration Constants | 配置常量 ---
// K-mer长度
constexpr int KMER_LEN = 6;
// 默认输出值
constexpr double DEFAULT_OUTPUT_VALUE = 10.0000;
// 每个碱基所需的位数，DNA有4种碱基(A,C,G,T)，需要2位二进制表示(00,01,10,11)
constexpr int BITS_PER_BASE = 2;
// K-mer空间大小，表示所有可能的K-mer总数
// 计算公式：4^KMER_LEN，使用位移操作实现：1左移(BITS_PER_BASE * KMER_LEN)位
constexpr std::uint64_t KMER_SPACE_SIZE   = 1ULL << (BITS_PER_BASE * KMER_LEN);  
// 前缀空间大小，表示所有可能的(K-1)-mer总数
// 计算公式：4^(KMER_LEN-1)，用于存储K-mer的前缀信息
constexpr std::uint64_t PREFIX_SPACE_SIZE = 1ULL << (BITS_PER_BASE * (KMER_LEN - 1)); 

// --- 编译时断言 ---
// 确保KMER_LEN为正数
static_assert(KMER_LEN > 0, "KMER_LEN must be positive");
//确保KMER_LEN不超过16，防止32位kmer编码溢出
static_assert(KMER_LEN <= 16, "KMER_LEN too large, would cause overflow for 32-bit kmer encoding");
// --- 类型定义 ---
// 使用32位无符号整数存储K-mer编码
using kmer_t  = std::uint32_t;
// 使用64位无符号整数存储K-mer出现次数
using count_t = std::uint64_t;

// 创建全局常量映射表，将DNA碱基字符转换为对应的整数编码
constexpr auto makeBaseToIntTable() {
    // 创建大小为256的数组，用于存储所有可能的ASCII字符到整数的映射
    std::array<std::int8_t, 256> t{};
    // 将数组所有元素初始化为-1，表示无效字符。注：这包括空格和制表符，我不太清楚fasta文件是否会包含这些字符。
    t.fill(-1);
    t[static_cast<unsigned char>('A')] = 0;  // 腺嘌呤(A)映射为0
    t[static_cast<unsigned char>('T')] = 1;  // 胸腺嘧啶(T)映射为1
    t[static_cast<unsigned char>('C')] = 2;  // 胞嘧啶(C)映射为2
    t[static_cast<unsigned char>('G')] = 3;  // 鸟嘌呤(G)映射为3
    t[static_cast<unsigned char>('a')] = 0;  // 小写a映射为0
    t[static_cast<unsigned char>('t')] = 1;  // 小写t映射为1
    t[static_cast<unsigned char>('c')] = 2;  // 小写c映射为2
    t[static_cast<unsigned char>('g')] = 3;  // 小写g映射为3
    return t;
}

// 使用constexpr确保在编译期计算，提高运行时性能
constexpr auto BASE_TO_INT = makeBaseToIntTable();

// 计算互补碱基的位编码
// 使用异或操作实现：0^1=1, 1^1=0, 2^1=3, 3^1=2
// 对应关系：A(0)<->T(1), C(2)<->G(3)
static inline int comp2bit(int b) { return b ^ 1; }
// 单向滚动计数器，用于高效计算DNA序列中的k-mer频率
struct RollingKmerCounter {
    kmer_t cur = 0;    // 当前k-mer的编码值
    int valid = 0;     // 当前有效碱基数量
    // 用于限制k-mer编码位数的掩码，确保编码值不超过KMER_LEN位
    static constexpr std::uint64_t MASK64 =
        (1ULL << (BITS_PER_BASE * KMER_LEN)) - 1ULL;
    // 重置计数器状态
    inline void reset() { cur = 0; valid = 0; }
    // 向前推进k-mer窗口，处理正向序列
    // 参数：
    //   base: 新加入的碱基编码(0-3)
    //   kmerCounts: k-mer计数数组
    //   prefixCounts: (k-1)-mer前缀计数数组
    inline void push_forward(int base,
                             std::span<count_t> kmerCounts,
                             std::span<count_t> prefixCounts) {
        // 将当前k-mer左移BITS_PER_BASE位，添加新碱基，并应用掩码
        cur = static_cast<kmer_t>(
            ((static_cast<std::uint64_t>(cur) << BITS_PER_BASE) |
             static_cast<std::uint64_t>(base)) & MASK64
        );

        // 当有效碱基数达到KMER_LEN时，更新计数器
        if (++valid >= KMER_LEN) {
            // 增加当前k-mer的计数
            ++kmerCounts[static_cast<std::size_t>(cur)];
            // 增加当前k-mer前缀的计数(去掉最后一个碱基)
            ++prefixCounts[static_cast<std::size_t>(cur >> BITS_PER_BASE)];
        }
    }

    // 更新反向互补序列的k-mer计数
    // 这个函数模拟在反向互补序列上从左到右滑动窗口，但实际上是在原始序列上从左到右读取。这是原始perl脚本的实现，我不想改动。
    inline void push_revcomp(int comp_base,
                             std::span<count_t> kmerCounts,
                             std::span<count_t> prefixCounts) {
        // 计算位移量，用于将新碱基放置在正确的位置
        constexpr int shift = BITS_PER_BASE * (KMER_LEN - 1);
        // 将当前k-mer右移BITS_PER_BASE位，添加新互补碱基到高位
        cur = static_cast<kmer_t>(
            (static_cast<std::uint64_t>(cur) >> BITS_PER_BASE) |
            (static_cast<std::uint64_t>(comp_base) << shift)
        );
        // 当有效碱基数达到KMER_LEN时，更新计数器
        if (++valid >= KMER_LEN) {
            // 增加当前k-mer的计数
            ++kmerCounts[static_cast<std::size_t>(cur)];
            // 增加当前k-mer前缀的计数(去掉最后一个碱基)
            ++prefixCounts[static_cast<std::size_t>(cur >> BITS_PER_BASE)];
        }
    }
};

// 双链流处理状态（正向链 + 反向互补链）
// 用于同时处理DNA序列的正向和反向互补序列的k-mer计数
struct DualStrandStream {
    RollingKmerCounter fwd;  // 正向链的k-mer计数器
    RollingKmerCounter rev;  // 反向互补链的k-mer计数器
    // 重置双向计数器状态
    inline void reset() { fwd.reset(); rev.reset(); }
    // 处理单个字符，更新正向和反向互补链的k-mer计数
    // 参数：
    //   ch: 输入的DNA字符
    //   kmerCounts: k-mer计数数组
    //   prefixCounts: (k-1)-mer前缀计数数组
    inline void push_char(char ch,
                          std::span<count_t> kmerCounts,
                          std::span<count_t> prefixCounts) {
        // 使用预定义的映射表将字符转换为碱基编码
        const int base = BASE_TO_INT[static_cast<unsigned char>(ch)];
        // 如果字符不是有效的DNA碱基(A,T,C,G)，则重置计数器
        if (base < 0) {
            // 无效字符会中断两个链的窗口（与旧行为保持一致）
            reset();
            return;
        }
        // 更新正向链的k-mer计数
        fwd.push_forward(base, kmerCounts, prefixCounts);
        // 更新反向互补链的k-mer计数：先计算互补碱基
        rev.push_revcomp(comp2bit(base), kmerCounts, prefixCounts);
    }
};

// 主函数，处理FASTA文件并计算k-mer频率
int main(int argc, char* argv[]) {
    // 检查命令行参数数量是否正确
    if (argc != 10) {
        std::cerr << "USAGE: " << argv[0]
                  << " [FASTA file] [id] [species] [genus] [family] [order] [class] [phylum] [outname]\n";
        return 1;
    }

    // 解析命令行参数
    const std::string fastaFile   = argv[1];  // FASTA文件路径
    const std::string id          = argv[2];  // 序列ID
    const std::string species     = argv[3];  // 物种名
    const std::string genus       = argv[4];  // 属名
    const std::string family      = argv[5];  // 科名
    const std::string order       = argv[6];  // 目名
    const std::string className   = argv[7];  // 纲名
    const std::string phylum      = argv[8];  // 门名
    const std::string outFileName = argv[9];  // 输出文件名（追加模式）
    // 检查k-mer空间大小是否超出size_t类型的限制
    if (KMER_SPACE_SIZE > std::numeric_limits<std::size_t>::max() ||
        PREFIX_SPACE_SIZE > std::numeric_limits<std::size_t>::max()) {
        std::cerr << "ERROR: (" << argv[0] << "): KMER_LEN (" << KMER_LEN
                  << ") is too large, resulting space exceeds size_t limits.\n";
        return 1;
    }
    // 计算k-mer和前缀的数组大小
    const std::size_t col_num        = static_cast<std::size_t>(KMER_SPACE_SIZE);
    const std::size_t prefix_col_num = static_cast<std::size_t>(PREFIX_SPACE_SIZE);
    // 初始化k-mer计数数组和前缀计数数组
    std::vector<count_t> kmerCounts(col_num, 0);
    std::vector<count_t> prefixCounts(prefix_col_num, 0);
    // 打开输入文件
    std::ifstream inputFile(fastaFile);
    if (!inputFile) {
        std::cerr << "ERROR! (" << argv[0] << "): Cannot access "
                  << fastaFile << ": No such file or directory\n";
        return 1;
    }
    // 打开输出文件（追加模式）
    std::ofstream outputFile(outFileName, std::ios::app);
    if (!outputFile) {
        std::cerr << "ERROR! (" << argv[0] << "): Cannot open "
                  << outFileName << " for appending.\n";
        return 1;
    }
    // FASTA文件解析，支持跨行连续性：
    // 在一个条目内保持跨行的滚动状态；遇到头部时重置
    std::string line;
    DualStrandStream ds;  // 双链流处理器
    ds.reset();           // 初始化状态
    // 逐行读取FASTA文件
    while (std::getline(inputFile, line)) {
        if (line.empty()) continue;  // 跳过空行
        // 如果是FASTA头部行（以'>'开头），重置状态
        if (line[0] == '>') {
            ds.reset(); // 新条目
            continue;
        }
        // 处理序列行中的每个字符
        for (char ch : line) {
            ds.push_char(ch, kmerCounts, prefixCounts);
        }
    }
    // 输出分类信息
    outputFile << id << '\t' << species << '\t' << genus << '\t'
               << family << '\t' << order << '\t' << className << '\t' << phylum;
    // 设置输出格式为固定小数点，精度为4位
    outputFile << std::fixed << std::setprecision(4);
    // 遍历所有可能的k-mer
    for (std::size_t j = 0; j < col_num; ++j) {
        // 计算当前k-mer的前缀索引
        const std::size_t prefix_idx = (j >> BITS_PER_BASE);
        assert(prefix_idx < prefix_col_num);
        // 如果前缀和k-mer都有计数，计算-log(p)值
        if (prefixCounts[prefix_idx] > 0 && kmerCounts[j] > 0) {
            // 计算概率：k-mer出现次数 / 前缀出现次数
            const long double p =
                static_cast<long double>(kmerCounts[j]) / static_cast<long double>(prefixCounts[prefix_idx]);
            // 输出-log(p)值
            outputFile << '\t' << -std::log(p);
        } else {
            // 如果没有计数，输出默认值
            outputFile << '\t' << DEFAULT_OUTPUT_VALUE;
        }
    }
    // 输出换行符，完成当前记录
    outputFile << '\n';
    return 0;
}