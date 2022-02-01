# esp32-vision 蒙德版外壳
注意：这个还没写完，先别弄  
UNDER CONSTRUCTION  

## 材料清单
注：这里的淘宝链接没有任何利益关系，只是为了方便说明。你可以随便买。
| 名称 							| 数量	| 备注					|
| :---							| :---		| :---					|	
| mondstadt-main.STL			| 1			| 本体 	|
| mondstadt-cap.STL				| 1			| 盖子					|
| 焊接调试好的电路板及屏幕		| 1			|						|
| 401120 或相近尺寸的锂电池		| 1			| 能不能塞得进去自己判断 |
| NTC 热敏电阻 10k B3380 		| 1			| 焊线的那种，粘在电池旁边测温度的 |
| M1.2\*3 螺丝 					| 4			| 可以多买一点备用						|
| FPC 排线 4P 1.5mm间距 3cm长 	| 1 		| 不是那种常见白色的，一定要耐高温（茶色），比如 [这个](https://item.taobao.com/item.htm?id=639292801110)	|
| TEMT6000X01 环境光传感器		| 1			| 焊到上面的排线上					|
| 无线充电用的隔磁片			| 若干		| 虽然下面模块会送，但看情况可能需要两层|
| Qi 协议的无线充电接收模块 	| 1			| 比如 [这个](https://item.taobao.com/item.htm?id=626465407103) ；买线圈大一点的版本 |
| 线圈							| 可选		| 接收模块送的线圈比较小，如果你使用下面提到的充电座则可以换一个稍大的线圈来提高效率；对于这些常见接收模块，选择电感量 10uH ~ 12uH，直径不超过 34mm 即可 |
| Qi 协议的无线充电发射模块		| 可选		| 外壳是按照 华为 GT2 Pro 的充电座的外形画的，那个充电器淘宝大概不到30，买来直接吸上就能用；也可以在接收模块同一家买|
| 35mm直径圆形的低弧的玻璃  	| 1			| 比如 [这个](https://item.taobao.com/item.htm?id=591580399917) ；建议多买一点，运输过程中可能撞出裂痕，需要挑选|
| 透明的胶水 					| 若干		| 需要能仅依靠外壳那一圈的面积粘得住那一大块玻璃，我用的是 维修佬 E8000 | 
| 8mm 直径， 1mm 厚的强磁铁		| 1			| 和 华为 GT2 Pro 充电器 一起使用时，用来吸附定位 |
| 自喷漆，金色（香槟金）		| 若干		| 外壳上色	|
| 自喷漆，光油					| 可选 		| 喷了有点脏，不喷金色容易掉，自己选		|
| 马克笔，金色					| 可选		| 用于补瑕疵；手残建议买，颜色需要和上面自喷漆一致	|
| 马克笔，黑色					| 1			| 用于给外壳上黑色的那几个位置涂色	| 
| 美纹纸胶带					| 若干		| 喷漆时用	|
| 导线							| 若干		| 那种多股漆包线比较好用，比如耳机线里的那种 |
| 砂纸 | 可选 | 用来打磨 3D 打印件上由于支撑带来的瑕疵，需要细一点的，我用的 2000 感觉粗了，建议 2000 5000 10000 各买一点 |

## 装配教程 
注意：下面的装配教程仅供参考，这里不提供任何标准装配流程。  
作为一个 DIY 项目，它当然充满瑕疵，过程中还可能会出现各种问题，还请自行发挥。  
作者保留最终解释权。  
（其实很多细节都和游戏中的模型有出入，暂时还没改的原因是因为 SolidWorks 水平太烂，SLDPRT 里变成屎山了，想改都没法改（（）  

### 外壳喷漆  
拿出 3D 打印的外壳，使用粗砂纸和细砂纸打磨表面至没有明显的支撑凸起；  
然后使用美纹纸胶带将不想被喷漆的位置覆盖好，然后使用自喷漆在表面薄薄地均匀地喷涂数次。如果上一步打磨过程中出现了奇怪的坑或者划痕，可以适当多喷一点点，但最忌心急喷一大堆。 

![](https://github.com/libc0607/esp32-vision/raw/main/img/mondstadt_main_painting.jpg)  
![](https://github.com/libc0607/esp32-vision/raw/main/img/liyue_painting.jpg)  

喷漆过程中请注意防护。  

喷光油的步骤可选，喷了的话外表显得有些脏，可能是我水平太烂（？）；不喷的话金色漆面可能容易掉。 

完成喷漆后，使用黑色马克笔把该涂成黑色的位置涂黑，即完成喷漆：  
![](https://github.com/libc0607/esp32-vision/raw/main/img/mondstadt_bare_with_glass.jpg)  

### 粘贴玻璃 
挑选一块没有裂痕没有瑕疵的 35 mm 直径低弧圆形玻璃。  
由于成本原因，它在运输过程中很容易出现刮花或者裂痕，所以建议多买一些到手后再挑；同样地，装配过程中也要小心碰撞。我买过两批，总体良率大概在一半左右。    

![](https://github.com/libc0607/esp32-vision/raw/main/img/mondstadt_bare_with_glass_and_glue.jpg)   

在外壳上均匀地挤一圈胶水，不要太多但是要能填满空隙，然后将玻璃放上去。注意要从多个侧面角度确认玻璃是否水平。  
放上玻璃后胶水会溢出一些，我是将浸湿酒精的无尘布裹在镊子头部擦掉它们的，你也可以自由发挥。  

![](https://github.com/libc0607/esp32-vision/raw/main/img/mondstadt_glass_glue.jpg)  

清理好溢出的胶水后，再次确认玻璃水平，使用美纹纸胶带将玻璃与外壳固定。我选择的这个胶水的固化大概需要一至两天，固化后暂时还没遇到过强度问题。  

![](https://github.com/libc0607/esp32-vision/raw/main/img/mondstadt_glass_glued_taped.jpg)  

### 安装环境光传感器  
准备一个 TEMT6000X01 环境光传感器，一条上表中提到的排线和胶水。  
将传感器如下图所示焊接在排线的引脚上。你可能需要像图上那样记录下引脚关系。  

![](https://github.com/libc0607/esp32-vision/raw/main/img/fpc_cable_with_temt6000_welded_02.jpg)  

然后小心地将传感器所在位置折弯 90° 如下：  

![](https://github.com/libc0607/esp32-vision/raw/main/img/mondstadt_fpc_90degree.jpg)  

在外壳的对应孔位挤一点胶水，并将传感器粘到孔中。  

![](https://github.com/libc0607/esp32-vision/raw/main/img/mondstadt_sensor_position.jpg)  

使用美纹胶固定好传感器，等待胶水固化。  

![](https://github.com/libc0607/esp32-vision/raw/main/img/mondstadt_sensor_position_taped.jpg)  

这时从外壳的正面应该可以正好露出传感器接收光线的部分。再在黑色部分均匀地覆盖一层胶水，同时作为固定和保护：  

![](https://github.com/libc0607/esp32-vision/raw/main/img/mondstadt_sensor_Aside_glue_filled_focus.jpg)  

至此你已完成了外壳主体部分的组装。在等待两天胶水固化后即可拆除所有胶带。  


