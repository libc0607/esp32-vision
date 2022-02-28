# ESP32 Genshin Impact Vision
用 ESP32 做个~~下北泽~~神之眼   
测试视频：  
[Bilibili BV1Fr4y1z7yS: 下北泽元素力测试 - bcccc23333](https://www.bilibili.com/video/BV1Fr4y1z7yS)  
[Bilibili BV1Fr4y1z7yS: 给刻师傅做个神之眼 - bcccc23333](https://www.bilibili.com/video/BV1ir4y1r7ZS)  

![image](https://github.com/libc0607/esp32-vision/raw/main/img/demo-mondstadt-yjsnpi.gif)  
![image](https://user-images.githubusercontent.com/8705034/155985863-6c168c1b-9d55-451a-8177-b0d1b43cfb1f.png)


## 硬件  
PCB 圆版：[OSHWHub: libc0607/esp32_vision_v3-2_gc9a01_rel](https://oshwhub.com/libc0607/esp32_vision_v3-2_gc9a01_rel)  
![image](https://user-images.githubusercontent.com/8705034/155985414-dc29f2b3-72e8-4fcf-86e9-d311e2b08795.png)  

PCB 方版：[OSHWHub: libc0607/esp32_vision_v3-2_liyuev3_st7789_rel](https://oshwhub.com/libc0607/esp32_vision_v3-2_liyuev3_st7789_rel)  
![image](https://user-images.githubusercontent.com/8705034/155985375-e8274fe1-dd50-4bb7-a1be-882611fffcee.png)  

## 软件
目前做了两版测试代码，可以实现基本功能，但都不算正式 Release。有待填坑   

[vision_SPIFFS_GIF_video](https://github.com/libc0607/esp32-vision/tree/main/src/vision_SPIFFS_GIF_video)： 不贴 SD Nand，仅使用 ESP32 内部的 SPIFFS 作为存储的版本  
[vision_sdcard_mjpeg](https://github.com/libc0607/esp32-vision/tree/main/src/vision_sdcard_mjpeg)： 使用 SD Nand 作为存储，支持 Wi-Fi 上传，支持 OTA 的版本  

## 外壳 
由于本人非专业，只在大二上过一晚上的 SolidWorks 课，原始工程的 SLDPRT 已经变成屎山，本着要脸的原则没有放出。  
但这里提供了 STL 格式的文件，可以直接拿去下单 3D 打印，想修改的话也可以逆向一下。  

另，现版本由于个人不算满意，可能会继续改动，持续到我删掉这句话为止；请自行考虑风险。  

[蒙德版外壳在这](https://github.com/libc0607/esp32-vision/tree/main/stl/mondstadt)   
[璃月版外壳在这](https://github.com/libc0607/esp32-vision/tree/main/stl/liyue)  
![image](https://user-images.githubusercontent.com/8705034/155986652-94c0bdcc-bc52-475f-8d96-ee1dc8aed9e1.png)  
![image](https://user-images.githubusercontent.com/8705034/155986832-7f6c0eb7-d1e6-46ee-b782-bc37fff176d0.png)  



## 你也可以看看 
https://oshwhub.com/Myzhazha/esp32_pico-d4-video 

## 许可
本设计采用 [CC BY-NC-SA 4.0](http://creativecommons.org/licenses/by-nc-sa/4.0) 许可。  
你当然可以自己做一个玩；NC 是为了防止潜在的mhy法务部警告（？）。

## 免责声明  
这东西仅为个人 DIY 项目，作者不保证所公开的内容绝对可用，也不可能解决所有出现的问题。  
你应该考虑到所有可能存在的风险，包括但不限于各种可能的人身伤害、金钱损失和 Emotional Damage。  
滥用本项目造成的一切负面效果与作者无关，最终解释权归作者所有。

