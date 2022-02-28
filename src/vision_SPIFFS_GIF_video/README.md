# vision_SPIFFS_GIF_video
这里是 ESP32 Vision 的测试代码之一。  
如果你只是想要一个可以简单地显示元素图标或者野兽仙贝，且不需要经常更改显示内容的钥匙链，那这个 Demo 适合你。   

## 测试效果 
见 [Bilibili BV1Fr4y1z7yS: 下北泽元素力测试 - bcccc23333](https://www.bilibili.com/video/BV1Fr4y1z7yS)  

## 硬件
这个例子没有使用到板上的 SD Nand，如果你想用这个例子那就可以不焊。   
这个例子通过修改代码的方式同时支持 方版（ST7789）和 圆版（GC9A01），修改方式见下文。  

## 功能
 - 上电后从 SPIFFS 读取 GIF 图片
 - 播放完成后进入深度睡眠
 - 通过双击唤醒或是定时器唤醒（代码中默认 30s），再播放一次 GIF 图片，如此循环
 - 如果电量低，它就不亮了 （？废话）

## 编译 & 上传  

1. 安装 Arduino  
2. 安装 [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)  （仅在 V1.0.6 下测试过；高版本和下面某个库的兼容不好）
3. 安装 [moononournation/Arduino_GFX](https://github.com/moononournation/Arduino_GFX)  
4. 安装 [bitbank2/JPEGDEC](https://github.com/bitbank2/JPEGDEC)  
5. 安装 [DFRobot/DFRobot_LIS](https://github.com/DFRobot/DFRobot_LIS)  
6. 安装 [me-no-dev/arduino-esp32fs-plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin)  
7. 下载这坨源码，打开
8. 选择：ESP32 Dev Module, 240MHz CPU, 80MHz Flash, DIO, 4MB(32Mb), No OTA (1M APP/3M SPIFFS), PSRAM Disable  
9. 上传
10. 将你需要的 GIF 文件通过 ffmpeg 或是其他的什么方式转为 240x240 @15fps，命名为 loop.gif ，放入 Data 文件夹中；这里也提供了两个示例 GIF 
11. "ESP32 Sketch Data upload" 写入 SPIFFS
12. 重启，你就获得了一颗下北泽神之眼 

## 自定义配置 
代码中有几个可以配置的地方： 
```
#define GIF_FILENAME "/loop.gif"      // SPIFFS 中的动图名，要以 / 开头


#define DEEP_SLEEP_TIME_S   30        // 深度睡眠后，定时器唤醒的时间，单位是秒


#define BAT_ADC_THRESH_LOW  1700      // 低电压保护。当每一轮工作开始时，如果 ADC 读取到的电压读数低于这个值，就不会播放 GIF，直接进入下一轮睡眠
                                      // 由于 ESP32 的 ADC 质量不咋样，所以没有换算成电压。你可以根据自己板子的情况尝试调节这个值。  


//Arduino_GC9A01  *gfx = new Arduino_GC9A01(bus, PIN_TFT_RST, 2, true);
Arduino_ST7789  *gfx = new Arduino_ST7789(bus, PIN_TFT_RST, 0, true, 240, 240, 0, 0);
                                      // 在这里选择你的屏幕种类

```
## 参考 
[moononournation/RGB565_video](https://github.com/moononournation/RGB565_video)  
雷元素图标来自 [Bilibili: 鱼翅翅Kira](https://space.bilibili.com/2292091)

## 免责声明  
这段代码仅用作测试，由于滥用造成的一切不好的后果和作者无关。
如果其中的图片素材涉及侵权，请联系我删除。
