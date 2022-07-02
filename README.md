# ESP32 神之眼 Genshin Impact Vision

[![Inazuma_Electro_Demo](https://res.cloudinary.com/marcomontalbano/image/upload/v1656770471/video_to_markdown/images/youtube--uiCPUxYVVak-c05b58ac6eb4c4700831b2b3070cd403.jpg)](https://www.youtube.com/watch?v=uiCPUxYVVak "Inazuma_Electro_Demo")  

[Bilibili BV1Fr4y1z7yS: 下北泽元素力测试](https://www.bilibili.com/video/BV1Fr4y1z7yS)  
[Bilibili BV1ir4y1r7ZS: 给刻师傅做个神之眼](https://www.bilibili.com/video/BV1ir4y1r7ZS)  
[Bilibili BV1hS4y1D7fo：蒙德外壳+圆版 测试](https://www.bilibili.com/video/BV1hS4y1D7fo)  
[Bilibili BV1a34y1s7b5：稻妻外壳+圆版 测试](https://www.bilibili.com/video/BV1a34y1s7b5)  
[Bilibili BV13S4y1e7Yu：蓝牙切换视频测试](https://www.bilibili.com/video/BV13S4y1e7Yu)  


**警告：如果你没有能够完全理解整个设计的能力，那就不推荐做。** 因为我也不知道又会遇到什么坑  

**这里不是 B 站 up [渣渣一块钱4个](https://space.bilibili.com/14958846) 的代码仓库**，别做错了。详情见下方 [消歧义补丁](https://github.com/libc0607/esp32-vision#%E6%B6%88%E6%AD%A7%E4%B9%89%E8%A1%A5%E4%B8%81) 一节  

如果你是第一次尝试电子制作，或是不确定能否在缺乏教程的情况下自行完成以 0402 元件为主的中高密度 PCB 焊接及调试，那么推荐你从 [渣渣一块钱4个：电子版璃月神之眼制作教程](https://www.bilibili.com/video/BV1HS4y1b7tQ) 做起，他的教程十分详细，难度和成本都比较低，可以快速做出一个能用的成品。  

另外我也有一个基于小渣渣的设计缩水的版本，使用他修改的外壳，基于 GPL 3.0 开源，对量产成本做了优化；  
见 [libc0607/vision-c3-youth](https://github.com/libc0607/vision-c3-youth)   
但我没有在量产，在卖的都不是我，所以我不是客服，遇到问题你自己看看源码（  

## 硬件特性
以下描述的是 V3.3 版本的特性：  
 - 1.54 寸 LCD（方版）或 1.28 寸 LCD （圆版）
 - ESP32-PICO-D4 核心，可以 Wi-Fi 上传
 - 内部显示的内容作为图片或者视频存在，可以在 SPIFFS 或 SD Nand 中存放图片/视频，理论上可以 128M ~ 8G  
 - 内置 LIS2DW12 加速度传感器，支持敲击识别，重力感应功能觉得有点鸡肋就删了  
 - 外壳预留 TEMT6000 环境光强度传感器位置，12-bit（大约） PWM 调光
 - 锂电池充电管理使用 CN3165，带有 NTC 保护，通过电阻配置来支持 4.2V/4.35V/4.4V
 - 圆版电池 R40350 3.85V/435mAh，方版电池 403035 3.7V/450mAh
 - 低功耗 DC-DC 降压 ETA3425，低电量时 100% 导通，即断电电压由锂电池保护板决定  
 - 用了一个 SY6280AAC 让屏和 SD Nand 支持断电 
 - 支持且仅支持无线充电，需要外接无线充电模块并将线圈贴在外壳上；配套使用 华为 GT2 Pro 的无线充电座即可充电，淘宝大概卖 29  
 - 本体带有一个物理按键，暂时用于切换显示模式
 - 支持将 iTag 蓝牙防丢器作为按钮控制视频切换
 
本设计的整体思路是尽量省电，虽然 V3.3 增加了电池容量（…），但受体积限制和屏幕拖累，续航还是尿崩  
你可以通过修改配置文件，在显示效果和续航间选择合适的平衡；详见 [vision_sdcard_mjpeg](https://github.com/libc0607/esp32-vision/tree/main/src/vision_sdcard_mjpeg)  

由于 2022 年 6 月的某个时候嘉立创将 ESP32-PICO-D4 踢出了经济型 SMT，考虑到少量自制的可制造性，未来本设计很可能会因此做出一些调整；  
但 V3.3 已经是一个相对稳定的版本，如果你等不及了可以做或是自己修改  
根据目前（2022.7.2）的[信息](https://t.me/paimon_leaks/1783)来看，须弥应该也是圆的，我猜电路大概是还能用的（吧…？），所以应该不用再大改了  

## PCB 设计  
PCB 圆版：   
V3.3: [OSHWHub: libc0607/esp32_vision_v3-3_gc9a01_rel](https://oshwhub.com/libc0607/esp32_vision_v3-3_gc9a01_rel)  

![image](https://user-images.githubusercontent.com/8705034/158412090-078fbdf3-1522-4b3c-a5c7-30ed71a7bd47.png)  

PCB 方版：  
V3.3: [OSHWHub: libc0607/esp32_vision_v3-3_st7789_rel](https://oshwhub.com/libc0607/esp32_vision_v3-3_st7789_rel)  
注：1.54 寸 ST7789 屏幕有很多厂家生产外形和引脚定义兼容的型号，但有一部分型号屏幕的侧面及背面漏光。  
由于把漆喷到一点都不漏光这件事不是很有操作性，所以推荐选择不漏光的版本；你可能需要问问卖家  
注2：屏幕可能存在色差，不同的屏色差不同，这个emmm就没什么低成本且可操作性强的解决方法，推荐把播放的视频用 AE 自己调个色  

![image](https://user-images.githubusercontent.com/8705034/161293365-5aa8db52-e6ec-49e8-a091-8577e36fbef4.png)


关于本设计在 2022 年 6 月之前的物料成本，仅供参考：以每批次做 5 套为例，约为 120 ~ 140 元/套。  
该价格：不含消耗品及工具等；不考虑良率；不含邮费；PCB 白嫖嘉立创；SMT 贴基础库；  
其中由于屏幕价格，方版整体会比圆版便宜；整体数值可能随元器件价格波动（不过大概只会高吧x   
懒人包：648做5个，做不出来再补一单328（？）  

2022 年 7 月更新：由于 JLC 把 ESP32-PICO-D4 移出了经济版 SMT 可贴列表，单次贴片的工程费由原本的 50 元提高到了 150+50 元。（我这邮寄的还没用完呢你就涨价草（  
除非你自己搞得定 ESP32-PICO-D4 的手工焊接，否则这会带来很高的成本提升。  

## 软件
目前做了两版测试代码，可以实现基本功能，但都不算正式 Release。有待填坑；  

[vision_SPIFFS_GIF_video](https://github.com/libc0607/esp32-vision/tree/main/src/vision_SPIFFS_GIF_video)： 不贴 SD Nand，仅使用 ESP32 内部的 SPIFFS 作为存储的版本  
[vision_sdcard_mjpeg](https://github.com/libc0607/esp32-vision/tree/main/src/vision_sdcard_mjpeg)： 使用 SD Nand 作为存储，支持 Wi-Fi 上传，支持 OTA，支持蓝牙的版本  

vision_SPIFFS_GIF_video 为早期版本的产物，没有维护，不能保证运行在新版硬件上。  
后续主要更新会集中在 vision_sdcard_mjpeg 上  

## 外壳 
由于本人非专业，只在大二上过一晚上的 SolidWorks 课，原始工程的 SLDPRT 已经变成屎山，本着要脸的原则没有放出。  
但这里提供了 STL 格式的文件，可以直接拿去下单 3D 打印，想修改的话也可以逆向一下，或是使用如 C4D 等可以修改的软件自行修改。  

另，现版本由于个人不算满意，可能会继续改动，持续到我删掉这句话为止；请自行考虑风险。  

[蒙德版外壳在这](https://github.com/libc0607/esp32-vision/tree/main/stl/mondstadt)   

[璃月版外壳在这](https://github.com/libc0607/esp32-vision/tree/main/stl/liyue)    

[稻妻的](https://github.com/libc0607/esp32-vision/tree/main/stl/inazuma)  

![image](https://user-images.githubusercontent.com/8705034/155986652-94c0bdcc-bc52-475f-8d96-ee1dc8aed9e1.png)   

![image](https://user-images.githubusercontent.com/8705034/155986832-7f6c0eb7-d1e6-46ee-b782-bc37fff176d0.png)   

![image](https://user-images.githubusercontent.com/8705034/161298378-ef804b8e-395f-4212-8504-cf7b54752a0f.png)  

## 参考 & 引用
两个软件例子都参考了 [moononournation/RGB565_video](https://github.com/moononournation/RGB565_video)   
以及 DFRobot_LIS 的 tapInterrupt 示例、ESP32 Arduino 中的 SDWebServer 和 OTAWebUpdater 示例  
蓝牙 iTag 部分参考自 [100-x-arduino.blogspot.com](http://100-x-arduino.blogspot.com/)  
元素图标来自 [Bilibili: 鱼翅翅Kira](https://space.bilibili.com/2292091)   
外壳的处理方法是从 @云梦泽创意空间 的群文件中学到的   
野兽先辈忘了从哪搞来的了。。  
如果还有遗漏请指出谢谢茄子  

## 消歧义补丁
**这里不是 B 站 up [渣渣一块钱4个](https://space.bilibili.com/14958846) 的代码仓库**，别做错了。  

我的设计和他的设计是**两套独立的设计，代码不通用，硬件不通用，外壳不通用**，开源协议也不同，只是碰巧外观都是神之眼。。  
如果你只是想要一颗挂件，被神明注视一下下，我推荐优先考虑他的设计，因为他的教程详细丰富，群里活人多，翻车严重的话还能买个他的核心板用；  
我的这边只有 Github 这点儿文档，还总是不更新，代码还有 bug，做了会被神明注释掉的（  

### A. [渣渣一块钱4个](https://space.bilibili.com/14958846) 的设计资料汇总
我这里仅作为参考，不对内容做保证，不保证完整性，以原作者为准，请自行检查是否有最新版。  
别做错了，别做混了，别问我哪里和哪里能不能通用。。。  
圆版 PCB：[OSHWHub: 神之眼挂件V1.1](https://oshwhub.com/Myzhazha/esp32_pico-d4-video)  
方版 PCB：[OSHWHub: 璃月神之眼挂件](https://oshwhub.com/Myzhazha/li-yue-shen-zhi-yan-gua-jian)  
方版教程：[渣渣一块钱4个：电子版璃月神之眼制作教程](https://www.bilibili.com/video/BV1HS4y1b7tQ)  
方版效果：[渣渣一块钱4个：璃月电子版神之眼](https://www.bilibili.com/video/BV1DY4y1b7GN)  
圆版教程：[渣渣一块钱4个：电子版神之眼（蒙德）开源说明](https://www.bilibili.com/read/cv15460352)  
圆版安装：[渣渣一块钱4个：神之眼圆形核心板安装教程](https://www.bilibili.com/video/BV1KB4y1m7H2)   
圆版效果：[渣渣一块钱4个：稻妻和蒙德电子版神之眼](https://www.bilibili.com/video/BV1sF411g7tc)    
外壳下单：[渣渣一块钱4个：神之眼外壳3D打印平台打印教程](https://www.bilibili.com/read/cv16117622)  
外壳喷漆：[渣渣一块钱4个：外壳喷漆教程](https://www.bilibili.com/video/BV1cY4y1a7CQ)   
视频格式转换：[渣渣一块钱4个：使用FFmpeg转换mjpeg格式](https://www.bilibili.com/read/cv15713513)  

他的 QQ 群：636426429  
他后续会制作大尺寸（大约你家墙上86盒那么大）的神之眼，而我暂时没有计划，如果需要可以关注他。  

### B. [我](https://space.bilibili.com/6509521) 的设计资料汇总
……就 全都在这 没别的了（   

### 我的设计和他的设计的关系

 - 电路部分各做各的，不通用，不过也有相互参考   
 - 程序部分各做各的，不通用  
 - 他的蒙德外壳和璃月外壳基于我的修改，但不通用
 - 他卖核心板（不含外壳），我不对外卖，所以想买的去找他
 - 他的外壳为 CC BY-NC-SA 协议，其余部分为 GPL 协议；我的全部设计为 CC BY-NC-SA 协议  



## 许可
本设计采用 [CC BY-NC-SA 4.0](http://creativecommons.org/licenses/by-nc-sa/4.0) 许可。（OSHWHub 上只能选 3.0 ..）  
我不卖成品，也不以任何形式卖套件，没精力搞。。  
但作为开源项目，你当然可以在遵守开源协议的情况下自己做一个玩；NC 是为了防止潜在的mhy法务部警告（？）。  
如果你还是充满好奇，可以看看 [《原神同人周边大陆地区正式运行指引V1.2》](https://weibo.com/ttarticle/p/show?id=2309404707028085113324)（不保证这里链接是最新版，请自己确认）  

## 免责声明  
这东西仅为个人 DIY 项目，作者不保证所公开的内容绝对可用，也不可能解决所有出现的问题。  
本设计的全部设计资料都已开源，这里的内容足够从零开始制作一个完全可用的成品，没有那种祖传的隐藏细节。  
如果你对组装细节有不清楚的地方，可以联系我补全，但我只会直接补全到 Github 上，以便他人参考。  
你应该考虑到所有可能存在的风险，包括但不限于各种可能的人身伤害、金钱损失和 Emotional Damage。  
滥用本项目造成的一切负面效果与作者无关，最终解释权归作者所有。

说真的，不推荐做，又费钱又费时间又容易翻车，还容易因为自己太菜而受到精神打击（ 
 
