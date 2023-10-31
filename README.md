[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-24ddc0f5d75046c5622901739e7c5dd533143b0c8e959d652212380cedb1ea36.svg)](https://classroom.github.com/a/W4MZkKgl)
# 2023-Lab2-RTP-Template

# Lab2 RTP

> 请从[北大教学网](https://course.pku.edu.cn)获取本Lab的邀请链接, DDL: `2023-12-15 23:30:00`

- - -

在lab2，你将会实现一个名为RTP（Reliable Transimission Protocol）的可靠传输协议。不同于lab1使用TCP来实现传输功能，UDP将作为RTP的基本传输API。为了实现RTP，你需要实现**建立连接、传输数据和终止连接**这三部分的功能，并且需要充分考虑信道可能出现的各种错误，包括**丢包、损坏、重复、乱序**等问题。

文档中的内容都很重要，请不要忽略其中的任何内容。当你有任何疑问的时候，**请先认真阅读文档**，教学团队不负责回答因为不认真阅读文档而产生的问题。然而，如果同学们发现文档、测评程序和框架代码有任何不合理或不清晰的地方，欢迎带着你们的思考和建议在**Piazza**上提问。

最后，网络课程的查重不是闹着玩的。请大家时刻记住：**不要抄袭！不要抄袭！不要抄袭！**

## 背景知识

### UDP — 最简单的传输层协议

UDP的使用和TCP类似。由于大家已经经历过lab1和实践课，这里只提供一个简单的、**不能直接运行的**示例：

```c
// the sender side
sock = socket(AF_INET, SOCK_DGRAM, 0);
memset(&dstAddr, 0, sizeof(&dstAddr));  // dstAddr is the receiver's location in the network
dstAddr.sin_family = AF_INET;
inet_pton(AF_INET, receiver_ip, &dstAddr.sin_addr);
dstAddr.sin_port = htons(receiver_port);

// the receiver side
sock = socket(AF_INET, SOCK_DGRAM, 0);
memset(&lstAddr, 0, sizeof(lstAddr));  // lstAddr is the address on which the receiver listens
lstAddr.sin_family = AF_INET;
lstAddr.sin_addr.s_addr = htonl(INADDR_ANY);  // receiver accepts incoming data from any address
lstAddr.sin_port = htons(port);  // but only accepts data coming from a certain port
bind(sock, &lstAddr, sizeof(lstAddr));  // assign the address to the listening socket
```

需要注意的是，**UDP不是TCP那样的可靠传输协议**，它不负责处理数据损坏或者丢失的情况。为了检查传输是否正确，你可以检查发出/收到的字节数是否与你的期望一致，也可以利用下一节将提到的checksum来验证数据的正确性。

如果你对于任何UDP接口或其他库函数的使用没有把握，例如你希望了解某个参数或返回值的含义，都可以使用**man**指令来查看这些函数的详细文档。

```bash
man sendto
man recvfrom
```

> Q: sendto/recvfrom各个参数的含义是什么？recvfrom和recv有什么区别？
>
> A: **RTFM** (**R**ead **T**he "**F**riendly" **M**anual): 假如你见到一个完全陌生的库函数，或者你已经忘记怎么使用某个库函数，又或者你希望使用一个库函数，却不知应该include哪个库。这种情况下，比起等待大神的回复，一行man xyz显然更快。上网查询当然也是一种可行的办法，但是网上的博客往往没有manual详细和全面。例如，很少有博客会详细描述库函数在出错时可能出现的所有错误码，以及它们对应的含义，然而有时这些细节对你调试代码很有帮助。因此，请学会主动阅读manual。

## 任务描述

你需要分别实现一个RTP协议的Sender（发送端）和Receiver（接收端）。

RTP的工作流程共分为三个阶段，分别是**建立连接、传输数据和终止连接**。首先，Sender向Receiver发起连接请求，二者尝试建立连接；在确认连接建立成功之后，Sender将指定文件的内容（仅传输文件存储的数据）逐步传输给Receiver，同时Receiver将文件写入指定文件；在文件数据传输完成之后，Sender主动终止连接，在确认连接终止之后二者停止运行。

在传输数据阶段，Sender和Receiver需要分别实现**回退N**和**选择重传**算法。此外，网络错误可能在**任意阶段**出现，你需要在实现中充分应对可能出现的差错。

本节将会详细介绍RTP协议的各部分。

### RTP协议头

首先，RTP的协议头结构定义如下：

```c
// rtp.h
typedef struct __attribute__((__packed__)) RtpHeader {
    uint32_t seq_num;  // Sequence number
    uint16_t length;   // Length of data; 0 for SYN, ACK, and FIN packets
    uint32_t checksum; // 32-bit CRC
    uint8_t flags;     // See at `RtpHeaderFlag`
} rtp_header_t;
```

对于各个字段的说明：

* `seq_num`：报文在传输序列中的编号。
* `length`：报文携带数据的字节数。对于部分不携带数据的报文，长度应该设置为0。
* `checksum`：基于32-bit CRC算法计算得出的校验码。
* `flags`：报文类型标志。它的高5位暂时未被使用（需要置0），低3位可以标识报文类型，具体使用方法请参考之后的部分。

为了简化操作，RTP头里的所有字段的字节序均采用小端法。

> Q: checksum字段有什么用？应该怎么设置这个字段？
>
> A: **RTFSC** (**R**ead **T**he "**F**riendly" **S**ource **C**ode): 许多问题的答案往往就藏在项目的源代码中。如果你不清楚某个字段或函数的用法，请先**认真**阅读它的源码和注释。比起被动的灌输知识，我们认为主动引导同学们思考能更好地帮助大家。你需要认真阅读util.c中的代码与注释，来学会如何正确地使用checksum字段。

### 建立连接

建立连接的过程与TCP协议的三次握手类似。

1. 第一次握手：Sender发送一个设置了`RTP_SYN`的报文（不携带数据），并且设置`seq_num`为一个随机的int值，记为`x`。
2. 第二次握手：Receiver在收到SYN报文后，向Sender发送一个同时设置了`RTP_SYN`和`RTP_ACK`的报文（表示建立连接并且确认收到SYN报文），并且将`seq_num`设置为`x+1`。
3. 第三次握手：Sender在收到SYNACK报文后，向Receiver回复一个`seq_num`为`x+1`的确认报文（设置RTP_ACK）。若Receiver成功收到该报文，则可以认为双方建立了RTP连接。

建立连接的过程可能受到各种网络错误的干扰，应对方法如下：

1. 如果Receiver在5s内没有收到Sender发送的SYN报文，则直接退出。
2. 假设一个报文需要收到对应的确认报文（设置`RTP_ACK`），如果报文发送方在等待100ms之后仍然没有收到对方的确认报文，则重传之前发送的报文。如果尝试次数超过了预设上限（设置得足够大即可，例如50），则报文的发送方可以直接退出。
3. 由于第三次握手不需要被再次确认，Sender可以等待一定时间（例如2s）来确认Receiver是否收到第三次握手。如果等待期间Sender再次收到SYNACK报文，说明Receiver还没有收到第三次握手，则Sender需要再次发送。如果等待中Sender没有再次收到SYNACK报文，则可以认为连接已经建立，Sender可以进入传输数据的阶段。
    > 这是相对简单的处理方法，也许你还能想到更好的解决方法。你可以尝试在**实现基本功能**的前提下，进行一些性能优化。
4. **直接丢弃checksum错误的报文**，在之后的任意阶段也类似。

> Q: 我不会写计时功能，怎么办？
>
> A: **STFW** (**S**earch **T**he "**F**riendly" **W**eb): 你并不需要使用特定的计时实现，只要功能正确即可。当你不知道如何实现某种功能时，可以尝试通过**Google/Bing**搜索，再结合manual来理解网络上的代码片段。如果你对某个问题毫无头绪，善用搜索可以给你提供很多思路。

### 数据传输（Receiver端）

在连接建立之后，Receiver将尝试接收数据报文、向Sender回复ACK报文，并将收到的数据报文写入文件。在这部分，你需要根据**回退N**和**选择重传**算法，实现**两种**Receiver发送ACK报文的策略。

#### 确认策略

Receiver需要维护一个滑动窗口，并且只接受编号在窗口内的数据报文。假设当前编号不大于`y`的数据报文都已被确认，且编号为`y+1`的数据报文未被确认，那么当前滑动窗口的编号范围是`[y+1, y+window\_size]`。如果Receiver收到了窗口“左侧”（编号小的一侧）一个或多个报文，则此时可以更新滑动窗口的范围。

当Receiver收到一个合法的数据报文，它将根据回退N或选择重传算法来发送ACK报文。这两种算法中ACK报文的`seq_num`字段有不同的含义：

* 对于回退N算法，`seq_num`代表Receiver希望收到的下一个报文的编号；
* 对于选择重传算法，`seq_num`表示Receiver收到了编号为`seq_num`的数据报文。

差错处理：

1. 如果等待5s没有收到任何报文，可以认为Sender已经断开连接，则Receiver可以直接退出；
2. 如果Receiver收到一个编号不在窗口范围内的数据报文，可以简单地丢弃这个报文；
3. 如果Receiver收到一个已经确认过的报文，根据回退N或选择重传算法重新发送ACK报文；
4. Receiver需要充分检查数据报文的正确性，包括`checksum`和`flags`等字段。当且仅当报文头中的字段全部正确，Receiver才应该处理这个报文。

### 数据传输（Sender端）

在连接建立之后，Sender将从文件读取数据，并向Receiver发送这些数据。与Receiver端类似，Sender也需要根据**回退N**和**选择重传**算法，实现**两种**传输数据的策略。

成功连接之后，Sender发送的第一个数据报文（flag为0）的`seq_num`应该为`x+1`（假设SYN报文的编号为`x`）。此后，每个新发送的数据报文（不包括重传）需要将编号递增1。Sender需要将文件数据进行适当的分片，并且根据滑动窗口算法逐渐将这些数据发送给Receiver，并且在适当的时机重传丢失或损坏的数据。

#### 分片

MTU（maximum transmission unit）决定了一个数据包的最大字节数，在linux系统中，这个值默认是1500字节。考虑到IP头有20字节、UDP头有8字节以及RTP头有11字节，可以计算出每个RTP数据包最多可以包含`1500-20-8-11=1461`字节。因此，你需要将文件分成多个数据包，并逐个地发送它们。

> Q: sendto/recvfrom似乎并没有发送字节数的限制，为什么还要分片？lab2并没有要求[像lab1那样用循环保证循环发出数据](https://edu.n2sys.cn/#/tut_lab/lv1/Socket&Epoll%E7%BC%96%E7%A8%8B%E5%85%A5%E9%97%A8)，为什么？
>
> A: 超出MTU的包会在IP协议中被分片，这会严重影响网络性能。并且，当我们通过UDP发出的包总是符合MTU的限制时，我们的包一定可以一次性发出，而不会被分片。所以在lab2的要求下，我们不需要靠循环检查来保证传输完成。然而，如果你的sendto/recvfrom返回了你不期望的特定数值，你仍然需要处理它们。

#### 发送策略

与Receiver类似，Sender也需要维护一个滑动窗口，窗口范围的计算方法相同。如果当前窗口内还有未发送的数据报文，Sender可以选择发送这些报文。

在Sender收到新的ACK之后，如果当前滑动窗口“左侧”（编号小的一侧）有一个或多个连续的数据报文得到确认，你将需要增加滑动窗口的下限，此时窗口的大小维持不变。你需要注意回退N和选择重传算法中，ACK报文编号的意义不同。

> 再次提醒：处理报文之前请先检查报文的正确性。

#### 超时重传

如果滑动窗口在一定的时间（100ms）内没有更新，Sender需要重传没有被确认的数据报文。

* 对于回退N算法，Sender每次重传当前窗口的所有报文。
* 对于选择重传算法，Sender只重传窗口内没有被确认的报文。

为方便实现，选择重传算法并不需要为窗口内的每个报文都维护一个计时器，只需要为整个窗口维护一个计时器即可。

### 终止连接

在数据传输完毕后，你需要断开Sender和Receiver之间的连接：

1. 第一次挥手：Sender发送一个带有标志`RTP_FIN`的报文，`seq_num`在最后一个数据报文的基础上增加1。
2. 第二次挥手：Receiver收到FIN报文后需要回复一个带有标志`RTP_FIN`和`RTP_ACK`的报文，并且`seq_num`和FIN相同。Sender收到该报文后，即认为连接终止。

这一阶段应对差错的策略和建立连接很类似，不再重复阐述，但请注意：

* Receiver在收到FIN报文之后不再处理其他类型的报文。
* Sender和Receiver在分别确认对方退出之后（收到特定类型报文或长时间未收到报文），才会终止运行。

### 具体实现

本节将具体描述lab的代码结构，以及测试程序与你实现的Sender和Receiver之间的交互方法。

#### 代码结构

* src/
  * receiver.c: Receiver的实现代码。你需要在这部分添加你的实现。
  * sender.c: Sender的实现代码。你需要在这部分添加你的实现。
  * rtp.h: RTP协议相关的定义。
  * util.h, util.c: 一些可能有用的辅助工具。
  * test.cpp: 测试程序。你可以改动这部分代码来进行个性化的本地测试，但最终评分以原始测试为准。
* third_party/: 第三方依赖。你不需要改动这部分代码。
* CMakeLists.txt: 项目构建文件。你可以自由地加入代码依赖，但请不要改变可执行文件的路径，否则测试将无法正常运行。

#### 交互方式

首先，你实现的Sender和Receiver将会被编译成可执行文件，并且在不同的进程运行。Sender和Receiver需要的各种参数，例如目的IP地址、端口、窗口大小、文件路径和使用的算法，都将通过命令行参数的形式传递给你的程序。

命令行参数的格式如下：

```bash
./sender [receiver ip] [receiver port] [file path] [window size] [mode]
./receiver [listen port] [file path] [window size] [mode]
```

具体地，`receiver ip`和`receiver port`分别代表接收端的地址和端口；`listen port`代表接收端监听的端口；`file path`代表Sender需要发送和Receiver需要写入的文件路径；`window size`代表滑动窗口的大小；`mode=0`代表使用回退N算法，`mode=1`代表使用选择重传算法。

> Q: Lab任务很复杂，没有头绪怎么办？
>
> **KISS法则** (**K**eep **I**t **S**imple and **S**tupid): 在往年的lab中，总有同学希望一次性写完全部的代码，然后再慢慢debug。但是在面对一些大型项目时，这种做法很容易一口气引入大量的bug，让debug过程变得苦不堪言。在实现复杂逻辑的时候，先从最基本的功能开始搭建，然后立马进行测试。保证这部分代码正确后，再去实现下一批功能。lab2的代码量或许不大，但它要实现的逻辑比较复杂，**按照KISS法则步步为营地推进，可以大大提高开发的效率**。

#### 调试

Lab2的测试存在随机因素，标准程序在与你的实现进行交互时，可能出现多种随机出现的错误。这种不确定性会大大提高debug的难度，因此有一个好的调试方法很重要。

相信你在过去完成lab的时候，曾多次使用printf大法来调试程序。这是很好的测试方法，但是在测试完成之后，过多的调试输出反而会导致程序运行缓慢；而逐行删除也是一份让人痛苦的工作。

因此，本次lab提供了使用宏开关的Debug Logging功能，你可以在CMakeLists.txt中控制是否开启该功能。

```cpp
// util.h
#ifdef LDEBUG
#define LOG_DEBUG(...)                                                  \
    do {                                                                \
        fprintf(stderr, "\033[40;33m[ DEBUG    ] \033[0m" __VA_ARGS__); \
        fflush(stderr);                                                 \
    } while (0)
#else
#define LOG_DEBUG(...)
#endif
```

当然，模板代码中还有其他的logging宏，你可以根据情况来使用它们。

## 测试

Lab2共有26个测试点，每个测试点的均为10分。其中，有6个隐藏测试点会在ddl之后公布，ddl前的测试满分仅有200分。最终通过所有26个测试点即可在本lab得到满分。

在ddl前公布的20个测试点中，有4个测试点不涉及网络错误，有8个测试点可能发生单个类型的错误，还有8个测试点可能发生多种错误。ddl后公布的6个隐藏测试点中，将会有相比于普通测试更大的传输数据、更高的故障发生率以及更大的滑动窗口尺寸。

测试用的标准Sender和Receiver可能产生以下四类传输错误：

1. 报文丢失
2. 报文内容错误
3. 报文重复
4. 报文乱序到达

### 测试流程

你需要使用模板代码中提供的CMake配置文件，将你的代码在项目下的build/目录编译成可执行文件，并且编译出测试程序。之后，测试程序会将你实现的Sender或Receiver程序与对应的标准程序进行交互，并检查RTP协议的各个工作阶段、以及最终的结果是否正确。

具体地，你可以执行下列指令进行测试：

```bash
cmake -B build/
cd build/
make
./rtp_test_all
```

如果希望运行单个测试点，可执行如下指令：

```bash
./rtp_test_all --gtest_filter=RTP.`测试点名称`
```

### 测试点描述

以下的表格给出了每一个测试点对应的ID和内容，注意，lab2只测试sender/receiver一对一交互的情况。

<table>
    <tr>
        <td>类别</td>
        <td>测试点名称</td>
        <td>分值</td>
        <td>是否在Deadline前放出</td>
        <td>测试点内容</td>
    </tr>
    <tr>
        <td rowspan="6">无差错</td>
        <td>NORMAL</td>
        <td>10</td>
        <td>是</td>
        <td>使用回退N，窗口大小为16，进行两次完整的无差错传输。第一次测试Sender，第二次测试Receiver，两次都通过才算通过测试</td>
    </tr>
    <tr>
        <td>NORMAL_SMALL_WINDOW</td>
        <td>10</td>
        <td>是</td>
        <td>使用回退N，窗口大小为1，进行两次完整的无差错传输。第一次测试Sender，第二次测试Receiver，两次都通过才算通过测试</td>
    </tr>
    <tr>
        <td>NORMAL_HUGE_WINDOW</td>
        <td>10</td>
        <td>否</td>
        <td>使用回退N，窗口大小为20000，进行两次完整的无差错传输。第一次测试Sender，第二次测试Receiver，两次都通过才算通过测试</td>
    </tr>
    <tr>
        <td>NORMAL_OPT</td>
        <td>10</td>
        <td>是</td>
        <td>使用选择重传，在窗口大小为16的前提下进行两次完整的无差错传输。第一次测试Sender，第二次测试Receiver，两次都通过才算通过测试</td>
    </tr>
    <tr>
        <td>NORMAL_OPT_SMALL_WINDOW</td>
        <td>10</td>
        <td>是</td>
        <td>使用选择重传，在窗口大小为1的前提下进行两次完整的无差错传输。第一次测试Sender，第二次测试Receiver，两次都通过才算通过测试</td>
    </tr>
    <tr>
        <td>NORMAL_OPT_HUGE_WINDOW</td>
        <td>10</td>
        <td>否</td>
        <td>使用选择重传，窗口大小为20000，进行两次完整的无差错传输。第一次测试Sender，第二次测试Receiver，两次都通过才算通过测试</td>
    </tr>
    <tr>
        <td rowspan="4">回退N Receiver</td>
        <td>RECEIVER_SINGLE_1</td>
        <td>10</td>
        <td>是</td>
        <td>使用回退N Receiver，窗口大小为16，故障率为10%，分别进行两次完整的传输流程。第一次传输仅含有丢包错误，第二次传输仅含有内容错误，两次传输都正确才算通过测试</td>
    </tr>
    <tr>
        <td>RECEIVER_SINGLE_2</td>
        <td>10</td>
        <td>是</td>
        <td>使用回退N Receiver，窗口大小为16，故障率为10%，分别进行两次完整的传输流程。第一次传输仅含有重复错误，第二次传输仅含有乱序错误，两次传输都正确才算通过测试</td>
    </tr>
    <tr>
        <td>RECEIVER_MIXED_1</td>
        <td>10</td>
        <td>是</td>
        <td>使用回退N Receiver，窗口大小为16，故障率为20%，只进行一次完整的传输流程。四种传输错误均可能出现</td>
    </tr>
    <tr>
        <td>RECEIVER_MIXED_2</td>
        <td>10</td>
        <td>是</td>
        <td>使用回退N Receiver，窗口大小为16，故障率为30%，只进行一次完整的传输流程。四种传输错误均可能出现</td>
    </tr>
    <tr>
        <td rowspan="4">回退N Sender</td>
        <td>SENDER_SINGLE_1</td>
        <td>10</td>
        <td>是</td>
        <td>使用回退N Sender，窗口大小为16，故障率为10%，分别进行两次完整的传输流程。第一次传输仅含有丢包错误，第二次传输仅含有内容错误，两次传输都正确才算通过测试</td>
    </tr>
    <tr>
        <td>SENDER_SINGLE_2</td>
        <td>10</td>
        <td>是</td>
        <td>使用回退N Sender，窗口大小为16，故障率为10%，分别进行两次完整的传输流程。第一次传输仅含有重复错误，第二次传输仅含有乱序错误，两次传输都正确才算通过测试</td>
    </tr>
    <tr>
        <td>SENDER_MIXED_1</td>
        <td>10</td>
        <td>是</td>
        <td>使用回退N Sender，窗口大小为16，故障率为20%，只进行一次完整的传输流程。四种传输错误均可能出现</td>
    </tr>
    <tr>
        <td>SENDER_MIXED_2</td>
        <td>10</td>
        <td>是</td>
        <td>使用回退N Sender，窗口大小为16，故障率为30%，只进行一次完整的传输流程。四种传输错误均可能出现</td>
    </tr>
    <tr>
        <td rowspan="6">选择重传 Receiver</td>
        <td>OPT_RECEIVER_SINGLE_1</td>
        <td>10</td>
        <td>是</td>
        <td>使用选择重传 Receiver，窗口大小为16，故障率为10%，分别进行两次完整的传输流程。第一次传输仅含有丢包错误，第二次传输仅含有内容错误，两次传输都正确才算通过测试</td>
    </tr>
    <tr>
        <td>OPT_RECEIVER_SINGLE_2</td>
        <td>10</td>
        <td>是</td>
        <td>使用选择重传 Receiver，窗口大小为16，故障率为10%，分别进行两次完整的传输流程。第一次传输仅含有重复错误，第二次传输仅含有乱序错误，两次传输都正确才算通过测试</td>
    </tr>
    <tr>
        <td>OPT_RECEIVER_MIXED_1</td>
        <td>10</td>
        <td>是</td>
        <td>使用选择重传 Receiver，窗口大小为16，故障率为20%，只进行一次完整的传输流程。四种传输错误均可能出现</td>
    </tr>
    <tr>
        <td>OPT_RECEIVER_MIXED_2</td>
        <td>10</td>
        <td>是</td>
        <td>使用选择重传 Receiver，窗口大小为16，故障率为30%，只进行一次完整的传输流程。四种传输错误均可能出现</td>
    </tr>
    <tr>
        <td>OPT_RECEIVER_MIXED_3</td>
        <td>10</td>
        <td>否</td>
        <td>使用选择重传 Receiver，窗口大小为32，故障率为60%，只进行一次完整的传输流程。四种传输错误均可能出现</td>
    </tr>
    <tr>
        <td>OPT_RECEIVER_MIXED_4</td>
        <td>10</td>
        <td>否</td>
        <td>使用选择重传 Receiver，窗口大小为32，故障率为50%，只进行一次完整的传输流程。可能出现丢包、损坏、乱序。</td>
    </tr>
    <tr>
        <td rowspan="6">选择重传 Sender</td>
        <td>OPT_SENDER_SINGLE_1</td>
        <td>10</td>
        <td>是</td>
        <td>使用选择重传 Sender，窗口大小为16，故障率为10%，分别进行两次完整的传输流程。第一次传输仅含有丢包错误，第二次传输仅含有内容错误，两次传输都正确才算通过测试</td>
    </tr>
    <tr>
        <td>OPT_SENDER_SINGLE_2</td>
        <td>10</td>
        <td>是</td>
        <td>使用选择重传 Sender，窗口大小为16，故障率为10%，分别进行两次完整的传输流程。第一次传输仅含有重复错误，第二次传输仅含有乱序错误，两次传输都正确才算通过测试</td>
    </tr>
    <tr>
        <td>OPT_SENDER_MIXED_1</td>
        <td>10</td>
        <td>是</td>
        <td>使用选择重传 Sender，窗口大小为16，故障率为20%，只进行一次完整的传输流程。四种传输错误均可能出现</td>
    </tr>
    <tr>
        <td>OPT_SENDER_MIXED_2</td>
        <td>10</td>
        <td>是</td>
        <td>使用选择重传 Sender，窗口大小为16，故障率为30%，只进行一次完整的传输流程。四种传输错误均可能出现</td>
    </tr>
    <tr>
        <td>OPT_SENDER_MIXED_3</td>
        <td>10</td>
        <td>否</td>
        <td>使用选择重传 Sender，窗口大小为32，故障率为60%，只进行一次完整的传输流程。四种传输错误均可能出现</td>
    </tr>
    <tr>
        <td>OPT_SENDER_MIXED_4</td>
        <td>10</td>
        <td>否</td>
        <td>使用选择重传 Sender，窗口大小为32，故障率为50%，只进行一次完整的传输流程。可能出现丢包、损坏、乱序。</td>
    </tr>
</table>

### 更多说明

下列是所有测试点（包括隐藏测试）都遵循的规则：

* 不需要考虑`seq_num`溢出的问题。
* 每次测试中，Sender和Receiver的窗口大小相同。
* 每次测试都只有一个Sender和一个Receiver，且二者使用的算法（回退N或选择重传）相同。
* 测试涉及的文件名的长度均不超过10个字符，且仅包含字母a-z。传输的数据大小不超过10MB，内容没有限制。
* 故障率为测试程序每次发包时产生错误的概率，每次发包是否出错之间相互独立。当多种错误可能混合出现时，每一类错误发生的概率相等。例如故障率20%且四种错误均可能出现，那么每一次发包时，发生错误的概率是20%，而发生某种特定错误的概率是`20% / 4 = 5%`。
* 故障仅由测试程序产生。例如对于同学们实现的Sender的测试，测试用Receiver将会在发出包的时候按照设定概率产生设定的故障。

## FAQ

### 我的代码可以在本地手动完成测试，但自动测试出错/可以在本地自动完成测试，但github测试出错……这类情况是不是意味着测试代码或文档写错了？

我们的lab测试程序和文档都经过教学团队的内部测试，如果同学们在开发中遇到了这类问题，**请先排查自己的代码和自己对实验文档的理解**是否有误。然而，如果你发现测试或文档确实存在一些问题，相信你已经做出了一些思考和尝试，**请在向我们指出这些问题的时候表明你的努力和推断**，这对我们尽快为同学们修复问题会有很大的帮助。

### 为什么助教总是让我上PIAZZA平台提问，是不想回答我的问题吗？

对于一门200人的高年级大课来说，私信解答的效率是**非常糟糕**的，因为在这样的选课人数下，同学们非常容易问出重复的问题。一个需要5分钟才能说清楚的问题，如果有20个同学都私信来问，就要花费100分钟来解答。但如果这个问题在piazza上发布了，只需要5分钟，我们就能让20个同学都解决这个问题。我们希望让同学们上piazza提问并**不是**拒绝和同学们沟通，恰恰相反，这能大大提高我们为同学们解答的效率，**节省同学们的时间**。换言之，学会上piazza提问能**让你的问题更快地被解答**。一个可以参考的提问流程如下：

> * 不要忘记[《提问的智慧》](https://github.com/ryanhanwu/How-To-Ask-Questions-The-Smart-Way/blob/main/README-zh_CN.md)，先自己做一些尝试，即便不能解决也可以排除一些可能性
> * 登入piazza，**先搜索**有没有人问过类似的问题
> * 点击New Post，编辑你的问题并发布。**遵循《提问的智慧》传授的方法能让你的问题更快被解答。**
> * piazza应该会自动开启邮件提醒，不要忘记及时回复那些帮助你的人（解答问题的并不一定是老师助教哦）

### 总是强调RTFM/RTFSC/STFW和《提问的智慧》，弄得我都不敢提问了

我们在lab中反复强调这些内容，是为了**引导**大家学习如何**自主解决问题**并且**更高效地提问**。我们并不否认，《提问的智慧》这篇文章的攻击性比较强，但这并不意味着我们想在同学们和教学团队之间建立起“禁止提问”的高墙。恰恰相反，《提问的智慧》提供了许多卓有成效的**自主解决问题的方法**和**高效的提问方式**，它们能帮助你在向教学团队提问之前快速搞定大部分在lab中遇到的问题。即便你在lab中不幸地遇到了一些不能独立解决的问题，这篇文章教你的提问方式也能帮助同学们更全面地描述自己的问题，进而更快地得到我们或者其他同学的帮助。不要小看它的作用，因为**助教为了解决不清晰的提问，需要反复追问更多的细节，这个聊天的过程一定会大大浪费你很多时间**，这肯定是你最不愿意看到的结果。
