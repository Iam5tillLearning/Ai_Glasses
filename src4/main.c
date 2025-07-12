#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>  // 添加缺失的头文件
// 自定义头文件（保持不变）
#include <jbd013_api.h>
#include <hal_driver.h>
#include <font.h>

#define SPI_DEVICE_PATH "/dev/spidev0.0"
#define SHM_NAME "/display_shm"       // 共享内存名称
#define SEM_NAME "/display_sem"       // 信号量名称
#define BUFFER_SIZE 128               // 消息缓冲区大小

int spi_file;
int display_inited = 0;  // 显示状态标记
int running = 1;         // 程序运行标记
sem_t *semaphore;        // 信号量指针
char *shared_memory;     // 共享内存指针

// 函数声明
int spi_init();
int setup();
void loop();
void cleanup(int signum);
void* display_update_thread(void* arg);

// SPI初始化（保持不变）
int spi_init() {
    if ((spi_file = open(SPI_DEVICE_PATH, O_RDWR)) < 0) {
        perror("Failed to open SPI device");
        return -1;
    }

    uint8_t mode = SPI_MODE_0;
    if (ioctl(spi_file, SPI_IOC_WR_MODE, &mode) < 0) {
        perror("Failed to set SPI mode");
        close(spi_file);
        return -1;
    }

    uint8_t bits = 8;
    if (ioctl(spi_file, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        perror("Failed to set SPI bits per word");
        close(spi_file);
        return -1;
    }

    uint32_t speed_hz = 19200000;
    if (ioctl(spi_file, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0) {
        perror("Failed to set SPI speed");
        close(spi_file);
        return -1;
    }

    int msb_first = 0;
    if (ioctl(spi_file, SPI_IOC_WR_LSB_FIRST, &msb_first) < 0) {
        perror("Failed to set SPI bit order");
        close(spi_file);
        return -1;
    }

    return 0;
}

// 显示更新线程：监听共享内存变化
void* display_update_thread(void* arg) {
    char last_message[BUFFER_SIZE] = {0};
    
    while (running) {
        // 等待信号量（有新消息）
        if (sem_wait(semaphore) == -1) {
            if (errno == EINTR) continue;  // 处理中断信号
            perror("sem_wait failed");
            break;
        }
        
        // 检查是否有新消息
        if (strcmp(shared_memory, last_message) != 0) {
            strncpy(last_message, shared_memory, BUFFER_SIZE - 1);
            
            // 处理"init"指令
            if (strcmp(shared_memory, "init") == 0 && !display_inited) {
                display_string("Inited");
                display_inited = 1;
                printf("Display updated to: Inited\n");
            } 
            // 处理其他显示内容
            else if (strcmp(shared_memory, "init") != 0) {
                display_string(shared_memory);
                printf("Display updated to: %s\n", shared_memory);
            }
        }
    }
    
    return NULL;
}

// 清理资源
void cleanup(int signum) {
    printf("\nCleaning up resources...\n");
    running = 0;
    
    // 释放共享内存和信号量
    if (shared_memory) {
        if (munmap(shared_memory, BUFFER_SIZE) == -1) {
            perror("munmap failed");
        }
    }
    
    if (semaphore) {
        sem_close(semaphore);
        sem_unlink(SEM_NAME);
    }
    
    shm_unlink(SHM_NAME);
    
    if (spi_file > 0) {
        close(spi_file);
    }
    
    exit(EXIT_SUCCESS);
}

// 初始化函数
int setup() {
    lv_init();          // 初始化LVGL图形库
    spi_init();         // 初始化SPI设备
    panel_init();       // 初始化面板

    // 初始显示"Not Inited"
    display_string("Not Inited");

    // 设置信号处理
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    // 创建并初始化共享内存
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        return -1;
    }
    
    if (ftruncate(shm_fd, BUFFER_SIZE) == -1) {
        perror("ftruncate failed");
        close(shm_fd);
        return -1;
    }
    
    shared_memory = mmap(0, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        perror("mmap failed");
        close(shm_fd);
        return -1;
    }
    
    close(shm_fd);
    memset(shared_memory, 0, BUFFER_SIZE);

    // 创建并初始化信号量
    semaphore = sem_open(SEM_NAME, O_CREAT, 0666, 0);
    if (semaphore == SEM_FAILED) {
        perror("sem_open failed");
        munmap(shared_memory, BUFFER_SIZE);
        shm_unlink(SHM_NAME);
        return -1;
    }

    // 创建显示更新线程
    pthread_t display_thread;
    if (pthread_create(&display_thread, NULL, display_update_thread, NULL) != 0) {
        perror("Failed to create display thread");
        return -1;
    }
    pthread_detach(display_thread);  // 线程后台运行

    printf("      ###################### 开 始 ######################      \n\n");
    printf("等待外部指令...（通过共享内存发送消息可更新显示）\n");
    printf("读温度传感器：\n");
    printf("%.2f℃\n", get_temperature_sensor_data());
    printf("\n############################ 结 束 ############################\n");

    return 0;
}

// 循环函数（按需保留）
void loop() {
    clr_char();
    usleep(100 * 1000);
}

// 主函数
int main() {
    if (setup() != 0) {
        return EXIT_FAILURE;
    }

    // 保持主程序运行
    while (running) {
        usleep(100000);  // 休眠100ms
    }

    return 0;
}