# FOC_Pocket
Utilizes the MA600A magnetic encoder, MP6541A integrated brushless motor driver, and ESP32-S3 microcontroller. The example includes position control, speed control, torque control, voltage sensing, and current sensing. The computation library is implemented with SimpleFOC
### 题外话
longlonglong time no seeeee!
工作之后好久没有开源东西了，做了好多项目在并行结果都进展缓慢。。。最近一个<u>**帅哥朋友**</u>说立创和MPS办了活动，问我有没有兴趣一起参加下，本想着简单浏览下MPS的芯片，结果瞄到了一款集成hall电流检测和三相半桥的无刷电机驱动器，又看了看前两天在某宝激情下单的野生6元2204云台电机--可以，瞌睡遇上枕头了。其实一直是想学习下BLDC电机和FOC控制的，奈何一直没时间，于是这次抽了两周时间和朋友趁下班时间熬夜边学边肝，肝出来一个**含MPS量极高的FOC控制器**，暂且称它**FOC_Pocket**
最后简单尝试了下FOC，记录了我学习的一些过程，感叹《自动控制原理》确实很好玩，只可惜大学时只对点屏感兴趣，后面也会用它好好学习的，然后再画一些平价的电机驱动板🤣~
***
## **完成了啥**
### **硬件**
* 电路设计
    芯片 datasheet 速览
    * [MP6541A](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MP6541AGQKT/document_id/10954/) -- 集成式bldc驱动芯片
    * [MPM3632SGPQ-Z](https://atta.szlcsc.com/upload/public/pdf/source/20240129/56B45157FCB7C765548052F4A0BA24C5.pdf) -- buck电源模块
    * [MA600A](https://item.szlcsc.com/datasheet/MA600GQE-0000-Z/19402713.html?spm=sc.gbn.xds.a&amp;lcsc_vid=RQJcAgVSQgAKUAdeT1ALA1wHQVcMUFZeFVBfVAYEFFIxVlNTRFlYVVBQQFNWUTsOAxUeFF5JWBIBSRccGwIdBEoFGAxBAAgJFQACSQwSGg0%3D) -- 磁编码器
    * [SIT3051TK](https://atta.szlcsc.com/upload/public/pdf/source/20200610/C601078_B1F4AB5D187DBA98E917965AB02FEC6C.pdf) -- can总线收发器
    * [ESP32-S3-PICO-1-N8R8](https://www.espressif.com.cn/sites/default/files/documentation/esp32-s3-pico-1_datasheet_cn.pdf) -- wifi/ble主控
* LAYOUT设计
* BOM器件标准化
* PCBA焊接
* 结构设计与装配
### **软件**
示例代码地址：[FOC_Pocket](https://github.com/HwzLoveDz/FOC_Pocket)
esp_simpleFOC库地址：[esp_simpleFOC](https://github.com/HwzLoveDz/espressif__esp_simplefoc)
* MA600A 磁编码器 spi 模式驱动
* 修改 esp_simpleFOC 库，添加支持 MA600A 磁编码器
* MP6541A 电机驱动芯片集成了三个hall电流传感器，位于每个桥臂的下管 MOSFET，可检测双向电流测量，在代码中实现 ADC 电压采样到电流的转换计算以及输出电流补偿
* 母线电压检测，硬件可通过分压电阻配置母线使能电压
* 位置开环/闭环控制
* 速度开环/闭环控制
* 力矩开环/闭环控制
* 力反馈旋钮(没调PID)
* CAN 收发器 self_test，用了官方的例子
### **成品展示**
![20250730-162756.255-1.jpg](https://image.lceda.cn/oshwhub/pullImage/d09b03f5ac154e8ea3569cf16135f858.jpg)
![20250730-162756.255-6.jpg](https://image.lceda.cn/oshwhub/pullImage/f035431e573d460fafb1fc681843025d.jpg)
![20250730-162756.255-5.jpg](https://image.lceda.cn/oshwhub/pullImage/405e6f6779074b54bf7d01062b47b47f.jpg)
![20250730-211105.jpg](https://image.lceda.cn/oshwhub/pullImage/fc3585f00d804e87931e4ed0e69a27c8.jpg)
## **详细介绍**
### **选型沟通**
因为是和<u>**帅哥朋友**</u>合作，所以用了某书云文档同步进展，合作选型，定期对齐进展，推荐大家也可以和朋友一起做一些项目，锻炼下表达能力以及和行业相关的工作能力，工作的时候方便自己也方便别人
![image.png](https://image.lceda.cn/oshwhub/pullImage/161825a767324f1bb3f30c579475e0bc.png)
![image.png](https://image.lceda.cn/oshwhub/pullImage/318368b6c39941a5ac052ecb23b6b182.png)
### **MP6541A软硬件设计简介**
#### **总结手册内容**
作为集成驱动器，可在最高输入电压达 40V 的电源电压下工作，8A 持续输出电流，电容泵支持 100% 占空比操作，具有低导通电阻、集成双向电流采样放大器和故障指示输出等特性。它集成了由6个 N 沟道功率 MOSFET 组成的三个半桥，其集成电流采样电路可以在每个桥臂的下管 MOSFET 中进行双向电流测量
所以我们最需要关注的地方就是如何从集成电流采样这里获得电流，其他的都由芯片帮我们解决了
#### **电流检测原理分析**
先看下芯片框图简单了解下内部运作的原理，芯片内部的电流检测电路可以实时监测三相输出中的电流。每个相位（Phase）都有一个对应的输出引脚，该引脚输出的电流大小与其所在相的电流成比例。需要注意的是：无论是正转还是反转，系统都只会检测低边MOS管（LS-FET）上的电流
![image.png](https://image.lceda.cn/oshwhub/pullImage/1392613f309d4e6b98fb84c978b96280.png)
要把这个电流信号转换成电压信号（比如输入给ADC模数转换器），需要在SOx引脚和参考电压（VREF）之间接一个电阻（RREF）
* 没电流时：输出端的电压（VSOUT）等于参考电压（VREF）
* 有电流时：输出电压（VSOUT）会高于或低于VREF，具体数值可以用公式计算
    ```C
    VSOUT = VREF + (RREF * ILOAD) / CURRENT_SENSE_RATIO
    ```
如果ADC的输入信号和供电电压成比例关系，可以在ADC的供电端和地之间，接两个阻值相同的电阻分压。零电流时，ADC的读数会停留在量程的中间值
![image.png](https://image.lceda.cn/oshwhub/pullImage/230232de09814ee9b24bec0f127cd003.png)
#### **电流检测硬件实现**
看了芯片内部框图之后我们看到电流检测使用放大器放大电压信号输出，所以看下这个放大器输出有没有需要注意的。看 ELECTRICAL CHARACTERISTICS 中有关电流采样的部分
![image.png](https://image.lceda.cn/oshwhub/pullImage/88a3535fb3c344669d7b0ff1439ca1d5.png)
* 最后一项 Current-sense minimum load impedance 限制了SOX 输出端参考电阻的阻值，上拉电阻需要大于1.8k，下拉电阻需要大于1.0k
* 还看到ABC三相的检测比率不一样，补偿电流也不一样，这些在计算的时候需要注意
* Current-sense output voltage swing 范围在0-5V，我们用的 ESP32-S3-PICO-1-N8R8 内部的ADC，最高能检测0-3.3V，因此需要缩小输出的范围，这里直接取对半，并且上下拉分压电阻需要大于手册要求的值，
    ![image.png](https://image.lceda.cn/oshwhub/pullImage/75b6fa0cc44f49b489cacdedf618779a.png)
* 为了防止可能出现的噪声，预留RC滤波
    ![image.png](https://image.lceda.cn/oshwhub/pullImage/c8f70d056845472fbcbe8539a0a9e400.png)
#### **电流检测软件实现**
写了例子，可以直接参考下，这里篇幅原因只解释大概
首先把我们需要的数据都定义出来
```CPP
#define CURRENT_SENSE_OFFSET_A -5.0f  // A相补偿，单位uA
#define CURRENT_SENSE_OFFSET_B -3.0f  // B相补偿，单位uA
#define CURRENT_SENSE_OFFSET_C -5.0f  // C相补偿，单位uA
#define CURRENT_SENSE_RATIO_A 11600.0 // A相电流检测比例
#define CURRENT_SENSE_RATIO_B 11500.0 // B相电流检测比例
#define CURRENT_SENSE_RATIO_C 11000.0 // C相电流检测比例
#define RREF 2495.0                   // 4.99k/2欧
#define VREF 3.3                      // 参考电压，单位V
```
ESP32S3的 ADC_ATTEN 设置为 ADC_ATTEN_DB_12 检测范围最大，所以我们初始化的时候要注意使用这个
```CPP
#define ADC_UNIT ADC_UNIT_1
#define ADC_ATTEN ADC_ATTEN_DB_11
#define ADC_SAMPLE_NUM 32   // 每次采样32次取平均
#define DEFAULT_VREF 0 // mV,只有ESP32可调,其它芯片从E-fuse获取

#define ADC_CHANNEL_NUM 3
#define ADC_CHANNEL_0 ADC1_CHANNEL_3 // GPIO4
#define ADC_CHANNEL_1 ADC1_CHANNEL_4 // GPIO32
#define ADC_CHANNEL_2 ADC1_CHANNEL_5 // GPIO33
```
ADC需要校准，刚开机的时候我们定义为0电流，所以刚开机的时候三相各读取一次作为初始值
```CPP
float vref_zero[ADC_CHANNEL_NUM] = {0}; // 三相静止时VSOUT校准值（单位V）

extern "C" void app_main(void)
{
    /.../
    // 启动时自动校准三相VREF_ZERO
    adc1_channel_t channels[ADC_CHANNEL_NUM] = {ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2};
    for (int ch = 0; ch &lt; ADC_CHANNEL_NUM; ++ch)
    {
        int sum = 0;
        for (int i = 0; i &lt; 100; ++i)
        {
            sum += adc1_get_raw(channels[ch]);
            vTaskDelay(1);
        }
        int raw = sum / 100;
        uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &amp;adc_chars); // 单位mV
        vref_zero[ch] = voltage / 1000.0f;                              // 单位V
    }
    /.../
}
```
注意到手册 Current-sense output offset current，这里需要做一下补偿
```CPP
while (1)
{
    /.../
    uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &amp;adc_chars); // 单位mV
    float vsout = voltage / 1000.0;                                 // 转为V
    float sense_ratio = (ch == 0) ? CURRENT_SENSE_RATIO_A : (ch == 1) ? CURRENT_SENSE_RATIO_B
                                                                      : CURRENT_SENSE_RATIO_C;
    float offset_uA = (ch == 0) ? CURRENT_SENSE_OFFSET_A : (ch == 1) ? CURRENT_SENSE_OFFSET_B
                                                                     : CURRENT_SENSE_OFFSET_C;
    /.../
}
```
电压转换为电流
```CPP
    // ILOAD = (VSOUT - VREF) * CURRENT_SENSE_RATIO / RREF
    float iload = (vsout - vref_zero[ch]) * sense_ratio / RREF; // 单位A
    float iload_mA = iload * 1000.0f + offset_uA / 1000.0f;     // 补偿后单位mA
```
然后让电机开环旋转就可以读到电流了
![hall_phase_current_det.png](https://image.lceda.cn/oshwhub/pullImage/dfd858470a6447feba042c4e4ff3b3b7.png)
### **MA600A软硬件设计简介**
#### **总结手册内容**
MA600A 是一款高精度、高带宽的磁角度传感器，可用于检测永磁体（通常为旋转轴上径向磁化的圆柱体）的绝对角度位置。MA600A 集成了精密隧道磁阻 (TMR) 传感器，可实现高带宽和高准确度 (INL)，其片上非易失性存储器 (NVM) 可存储配置参数，如参考零角度位置以及 ABZ、UVW 和脉宽调制 (PWM) 设置
MA600A 经过工厂校准，在全温工作范围内误差 (INL) 低于 0.6°。此外，用户还可通过可配置32 点校正表进行最终系统校准；用户校准后的最终误差 (INL) 可小于 0.1°
与 MA600A的通信可通过串行外设接口 (SPI) 和同步串行接口 (SSI) 完成
该器件还支持菊花链配置，通过 SPI 可依次读取多个传感器的角度，这最大限度地减少了控制器设备使用的 I/O 引脚数量
所以我们最需要关注的地方就是怎么搓驱动。。。
#### **驱动部分**
因为要使用esp_simpleFOC库，因此驱动格式需要适配，需要继承其 SENSOR 类，override其中必要的获取角度的函数，所以其它什么校准补偿、NVM设置之类的我们先不关注，只需关注如何获取角度
芯片支持SSI和SPI，我们使用SPI，所以关注下SPI的频率、模式和时序图
![image.png](https://image.lceda.cn/oshwhub/pullImage/28b2ba2e8aed4c02ab602ef700004f9f.png)
看到SPI的频率最高支持到25MHz
![image.png](https://image.lceda.cn/oshwhub/pullImage/63464db8f59643888565ab70ecb478ac.png)
使用SPI模式3，需要在驱动初始化的时候设置，MA600A 会自动检测SPI模式
支持八种类型的SPI操作：
• 读取角度 
• 读取多圈 
• 读取速度 
• 读取寄存器 
• 写入寄存器 
• 将单个寄存器块存储到NVM 
• 从NVM恢复所有寄存器块 
• 清除错误标志 
看下手册角度怎么获取：完整的角度读数需要16个时钟脉冲。角度输出值是先读取最高有效位（MSB）
![image.png](https://image.lceda.cn/oshwhub/pullImage/0c3d8149a3eb4da1ab8b3935e4f29e16.png)
然后通过这个转化为deg角度
![image.png](https://image.lceda.cn/oshwhub/pullImage/c2437813a29c47f1b4c304c14c35cc31.png)
但是驱动里需要转为rad角度，使用如下公式：
```CPP
angle = raw_angle * 2 * PI / 65536.0f
```
最后简单写下收发就可以了
```CPP
uint16_t MA600A::readRawPosition() {
    if (!_is_installed) {
        ESP_LOGE(TAG, "SPI not initialized");
        return 0;
    }

    // SPI transaction for reading angle (command: 0x0000)
    uint8_t tx_buffer[2] = {0x00, 0x00}; // Command for angle reading
    uint8_t rx_buffer[2] = {0};
    
    spi_transaction_t t = {};
    t.length = 16; // 16 bits
    t.tx_buffer = tx_buffer;
    t.rx_buffer = rx_buffer;
    
    esp_err_t ret = spi_device_polling_transmit(_spi_device, &amp;t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI transmit failed: %s", esp_err_to_name(ret));
        return 0;
    }
    
    // Combine received bytes into 16-bit value
    return (rx_buffer[0] &lt;&lt; 8) | rx_buffer[1];
}

float MA600A::getSensorAngle() {
    uint16_t raw_angle = readRawPosition();
    float angle = raw_angle * COUNTS_TO_RADIANS;    // Converts raw data into angle information in radians.
    return angle;
}
```
手册其他部分主要是误差校准，实际上什么都不做都有±0.04左右的精度，这个还是非常爽的
使用位置和速度控制，观察磁编码器执行的精度和稳定性
![PixPin_2025-07-30_02-05-25.png](https://image.lceda.cn/oshwhub/pullImage/c423d6794c724bc7b1e6d632702861d0.png)
芯片内部默认是有滤波器的，所以角度数据不需要做滤波了，咱们默认使用窗口5哈
![image.png](https://image.lceda.cn/oshwhub/pullImage/5bbfa1afd50c4337a291ac4ba9b01126.png)
如果你需要用MPS家的磁编，需要仿真可以用这个网站：https://sensors.monolithicpower.com/magnetselection
给一下这个2204电机磁环仿真的参数：磁环材质（NdFeB）
![image.png](https://image.lceda.cn/oshwhub/pullImage/ffa9d777da4f4bde96f72f7c27ca7917.png)
### **MPM3632SGPQ-Z buck电源模块**
#### **总结手册内容**
MPM3632S是一款具有内置功率MOSFET、一个电感和两个电容的同步整流降压迷你模块稳压器。它提供了一种紧凑的解决方案，4V 至 18V 输入范围，仅需输入和输出电容即可实现3A的持续输出电流，并在广泛的输入电源范围内提供出色的负载和线路调节。MPM3632S在固定的2.2MHz开关频率下工作，采用恒定导通时间（COT）控制，以提供快速的负载瞬态响应。完整的保护特性包括输出过压保护、过流保护和热关断
这个模块用来把母线电压转换为VIO 3.3V电平，因为电机芯片支持到40V输入，这里看电机是扛不住这么高电压，顶多上个3S应该就够了，而且电机功率较低，USB 5V也可以方便驱动，所以这款电压范围很合适，并且可以尽可能减少布局面积
#### **注意注意**
硬件可通过分压电阻配置母线使能电压，默认配置为2S锂电池放电低点（7.2V），防止过放。如果不需要，或者希望通过USB电源直接驱动电机，就去掉这个分压电阻，否则电源芯片不使能
![image.png](https://image.lceda.cn/oshwhub/pullImage/1d2ac73795404929b38805e4549b1934.png)
### **FOC部分**
![image.png](https://image.lceda.cn/oshwhub/pullImage/e82add500d074cfca756a48b7bf02b8e.png)
#### **ESP-IDF组件设置**
由于使用了MA600A，所以我 fork 了下[官方的esp_simpleFOC库](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/motor/foc/esp_simplefoc.html)，更新之后的地址：[esp_simpleFOC](https://github.com/HwzLoveDz/espressif__esp_simplefoc)，如果需要在自己项目中使用，在idf_component.yml文件中加入如下代码
```yml
dependencies:
  hwzlovedz_esp_simplefoc:
    git: https://github.com/HwzLoveDz/espressif__esp_simplefoc.git
    version: "*"

  ## Required IDF version
  idf:
    version: '&gt;=4.4'
  # # Put list of dependencies here
  # # For components maintained by Espressif:
  # component: "~1.0.0"
  # # For 3rd party components:
  # username/component: "&gt;=1.0.0,&lt;2.0.0"
  # username2/component2:
  #   version: "~1.0.0"
  #   # For transient dependencies `public` flag can be set.
  #   # `public` flag doesn't have an effect dependencies of the `main` component.
  #   # All dependencies of `main` are public by default.
  #   public: true
```
编译时会自动从git上拉取，不需要手动操作，如果不知道为什么可以这样，参考我写的这个博客：[[经验分享]ESP-IDF:如何向ESP Component Registry上传自己的组件或从Github拉取组件](https://blog.csdn.net/mondraker/article/details/149145442?fromshare=blogdetail&amp;sharetype=blogdetail&amp;sharerId=149145442&amp;sharerefer=PC&amp;sharesource=mondraker&amp;sharefrom=from_link)
#### **电机和磁编码器设置**
主要是这个2204电机的一些参数，初始化电机需要用到
如果对电机不熟悉可以简单看看这个：[esp-iot-solution bldc_overview](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/motor/bldc/bldc_overview.html)
```CPP
/**
 BLDCMotor class constructor
 @param pp pole pairs number
 @param R  motor phase resistance - [Ohm]
 @param KV  motor KV rating (1/K_bemf) - rpm/V
 @param L  motor phase inductance - [H]
 */ 
BLDCMotor(int pp,  float R = NOT_SET, float KV = NOT_SET, float L = NOT_SET);
```
按照doxygen注释测量数据并且填入
pp：极对数，转子上磁钢的数量除以 2，可以通过给任意两相通过小电压，手动旋转电机一周，感受阻力的次数就是极对数。如感到 6 次阻力，极对数就是 6，这里填 14/2
R：电机相电阻，万用表测电机两项电阻 RL，相电阻为其一半，这里填 6.5/2
KV：电机 KV 值，可以直观表示无刷电机在具体工作电压下的具体转速，这里填 330
L：电机相电感，电机静止时的定子绕组两端的电感为 LL, 相电感为其一半，使用电桥测量，这里填 (1.2333e-3) / 2
```CPP
BLDCMotor motor = BLDCMotor(14 / 2, 6.5 / 2, 330, (1.2333e-3) / 2);
BLDCDriver6PWM driver = BLDCDriver6PWM(11, 14, 12, 15, 13, 16, 17, 0);
MA600A ma600a = MA600A(SPI2_HOST, GPIO_NUM_21, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9);
```
#### **初始化FOC**
其它就非常简单了，直接调库设置，比如使用位置速度环控制
```CPP
// initialise magnetic sensor hardware
ma600a.init();
// link the motor to the sensor
motor.linkSensor(&amp;ma600a);

// driver config
// power supply voltage [V]
driver.voltage_power_supply = 8.4;
driver.voltage_limit = 7.4;
// pwm frequency [Hz]
// 20kHz is the default value
driver.pwm_frequency = 50000;
driver.init();
// link the motor to the driver
motor.linkDriver(&amp;driver);

// choose FOC modulation (optional)
motor.foc_modulation = FOCModulationType::SpaceVectorPWM;

// set motion control loop to be used
motor.controller = MotionControlType::angle;

// contoller configuration
// default parameters in defaults.h

// velocity PI controller parameters
motor.PID_velocity.P = 0.03f;
motor.PID_velocity.I = 1;
motor.PID_velocity.D = 0.0001f;
// maximal voltage to be set to the motor
motor.voltage_limit = 2.0;

// velocity low pass filtering time constant
// the lower the less filtered
motor.LPF_velocity.Tf = 0.01f;

// angle P controller
motor.P_angle.P = 20;
// maximal velocity of the position control
motor.velocity_limit = 6.28 * 3;

// initialize motor
motor.init();
// align sensor and start FOC
motor.initFOC();
```
然后在它转的时候堵他，放开之后他会回到正在执行的位置
![位置闭环.png](https://image.lceda.cn/oshwhub/pullImage/93c47f0fe1864e7e9f471ce2b06b0f6c.png)
### **CAN总线**
这部分软件只做了测试，验证CAN收发器芯片没有问题，未来开发多设备通信
![2934f501e6a463579a2ff9ea994e392.png](https://image.lceda.cn/oshwhub/pullImage/6ed3b604b5a34838a3dec4c7b755b5b9.png)
### **模型设计简介**
我感觉没有啥设计。。。就是能固定就行了
#### **模型**
![image.png](https://image.lceda.cn/oshwhub/pullImage/bc7b0dd54ee4494d9ef486647145d226.png)
#### **实物**
![image.png](https://image.lceda.cn/oshwhub/pullImage/ad7e15ff8a484511a97b9e45c0890d26.png)
![image.png](https://image.lceda.cn/oshwhub/pullImage/86e520531baf4098a1808f41174d5b6c.png)
## **装配部分**
### **PCB和BOM**
PCB直接下单就行了，免费的
BOM也是直接下单即可，已经做了器件标准化，但是有几颗MPS的芯片买不到，得找官方。。。
贴片还是比较考验的，特别是ESP32S3-PICO的LGA封装
### **电机**
电机在哪买呢，电机在淘宝搜2204电机直接出来一堆，买不带限位的或者自己回来磨掉限位柱
![image.png](https://image.lceda.cn/oshwhub/pullImage/713e3b42c07341a78a0e7c570c02e26e.png)
### **螺柱螺丝**
![img_v3_02om_25c1a311-bd2a-41a6-a903-c94ed456787g.jpg](https://image.lceda.cn/oshwhub/pullImage/8ab9bbad00d24339ac7a5e4c711509ba.jpg)
### **安装过程**
3mm和4mm螺柱这样叠起来，可以消除PCB螺丝孔和电机安装孔的误差，然后注意磁编气隙大概在1mm左右
![img_v3_02om_03f9a0ec-d1a1-4cf7-9e3f-14dde5f4cc7g.jpg](https://image.lceda.cn/oshwhub/pullImage/70d736baadd94675bd649be14677a013.jpg)
然后给外壳装上就行了，非常简单
## **最后**
写文档比画PCB时间都长。。。
如有笔误敬请指正，欢迎进群，交流学习！
群号：735791683
点赞收藏，你的鼓励是我开源的动力！
