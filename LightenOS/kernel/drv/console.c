#include <xtos.h>

#define NR_PIX_X 1280
#define NR_PIX_Y 800
#define CHAR_HEIGHT 16
#define CHAR_WIDTH 8
#define NR_CHAR_X (NR_PIX_X / CHAR_WIDTH)
#define NR_CHAR_Y (NR_PIX_Y / CHAR_HEIGHT)
#define NR_BYTE_PIX 4
#define VRAM_BASE (0x40000000UL | DMW_MASK)
#define L7A_I8042_DATA (0x1fe00060UL | DMW_MASK)
#define BUFFER_SIZE PAGE_SIZE

//每毫秒循环次数（适配高速CPU，初始设为100万，可微调）
#define LOOP_PER_MS 1000000UL   // 每毫秒循环次数（可调整：值越大延时越长）
void delay_ms(unsigned int ms)
{
    // 双层循环提高延时精度，避免编译器优化
    for (unsigned int i = 0; i < ms; i++)
    {
        for (unsigned int j = 0; j < LOOP_PER_MS; j++)
        {
            __asm__ volatile ("nop");  // 空指令，防止编译器优化循环
        }
    }
}
struct queue
{
	int count, head, tail;
	struct process *wait;
	char *buffer;
};

int def=0;//shift flag

int isFirst=1;//记录是否是第一次移动，清除数据污染
char* save=(char *)0x90000000403e8001;//显示器最大输出范围为403e8000，该空间不被使用
char* temp;//移动赋值的指针
char *pos;
extern char fonts[];
struct queue read_queue;

int sum_char_x[NR_CHAR_Y];
static int add = 1;    // 闪烁计数阈值
static int flag = 0;   // 光标显示/隐藏标志
static int x , y; // 光标坐标



char digits_map[] = "0123456789abcdef";
char keys_map[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '`', 0,
	0, 0, 0, 0, 0, 'q', '1', 0, 0, 0, 'z', 's', 'a', 'w', '2', 0,
	0, 'c', 'x', 'd', 'e', '4', '3', 0, 0, 32, 'v', 'f', 't', 'r', '5', 0,
	0, 'n', 'b', 'h', 'g', 'y', '6', 0, 0, 0, 'm', 'j', 'u', '7', '8', 0,
	0, ',', 'k', 'i', 'o', '0', '9', 0, 0, '.', '/', 'l', ';', 'p', '-', 0,
	0, 0, '\'', 0, '[', '=', 0, 0, 0, 0, 13, ']', 0, '\\', 0, 0,
	0, 0, 0, 0, 0, 0, 127, 0, 0, 0, 0, 0, 0, 0, '`', 0};
char keys_map_1[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '`', 0,
    0, 0, 0, 0, 0, 'Q', '1', 0, 0, 0, 'Z', 'S', 'A', 'W', '2', 0,
    0, 'C', 'X', 'D', 'E', '4', '3', 0, 0, 32, 'V', 'F', 'T', 'R', '5', 0,
    0, 'N', 'B', 'H', 'G', 'Y', '6', 0, 0, 0, 'M', 'J', 'U', '7', '8', 0,
    0, ',', 'K', 'I', 'O', '0', '9', 0, 0, '.', '/', 'L', ';', 'P', '-', 0,
    0, 0, '\'', 0, '[', '=', 0, 0, 0, 0, 13, ']', 0, '\\', 0, 0,
    0, 0, 0, 0, 0, 0, 127, 0, 0, 0, 0, 0, 0, 0, '`', 0
};

void write_char(char ascii, int xx, int yy)
{
	char *font_byte;
	int row, col;
	char *pos;

	font_byte = &fonts[(ascii - 32) * CHAR_HEIGHT];
	pos = (char *)(VRAM_BASE + (yy * CHAR_HEIGHT * NR_PIX_X + xx * CHAR_WIDTH) * NR_BYTE_PIX);
	for (row = 0; row < CHAR_HEIGHT; row++, font_byte++)
	{
		for (col = 0; col < CHAR_WIDTH; col++)
		{
			if (*font_byte & (1 << (7 - col)))
			{
				*pos++ = 255;
				*pos++ = 255;
				*pos++ = 255;
				*pos++ = 0;
			}
			else
			{
				*pos++ = 0;
				*pos++ = 0;
				*pos++ = 0;
				*pos++ = 0;
			}
		}
		pos += (NR_PIX_X - CHAR_WIDTH) * NR_BYTE_PIX;
	}
}

void erase_char(int xx, int yy)
{
	int row, col;
	char *pos;

	pos = (char *)(VRAM_BASE + (yy * CHAR_HEIGHT * NR_PIX_X + xx * CHAR_WIDTH) * NR_BYTE_PIX);
	for (row = 0; row < CHAR_HEIGHT; row++)
	{
		for (col = 0; col < CHAR_WIDTH; col++)
		{
			*pos++ = 0;
			*pos++ = 0;
			*pos++ = 0;
			*pos++ = 0;
		}
		pos += (NR_PIX_X - CHAR_WIDTH) * NR_BYTE_PIX;
	}
}
void scrup()
{
	int i;
	char *from, *to;

	to = (char *)VRAM_BASE;
	from = (char *)(VRAM_BASE + (CHAR_HEIGHT * NR_PIX_X * NR_BYTE_PIX));
	for (i = 0; i < (NR_PIX_Y - CHAR_HEIGHT) * NR_PIX_X * NR_BYTE_PIX; i++, to++, from++)
		*to = *from;
	for (i = 0; i < NR_CHAR_X; i++)
		erase_char(i, NR_CHAR_Y - 1);
	for (i = 0; i < NR_CHAR_Y - 1; i++)
		sum_char_x[i] = sum_char_x[i + 1];
	sum_char_x[i] = 0;
}
void cr_lf()
{
	x = 0;
	if (y < NR_CHAR_Y - 1)
		y++;
	else
		scrup();
}
void del()
{
	if (x)
	{
		x--;
		sum_char_x[y] = x;
	}
	else if (y)
	{
		sum_char_x[y] = 0;
		y--;
		x = sum_char_x[y];
	}
	erase_char(x, y);
}
void printk(char *buf)
{
	char c;
	int nr = 0;

	while (buf[nr] != '\0')
		nr++;
	erase_char(x, y);
	while (nr--)
	{
		c = *buf++;
		if (c > 31 && c < 127)
		{
			write_char(c, x, y);
			sum_char_x[y] = x;
			x++;
			if (x >= NR_CHAR_X)
				cr_lf();
		}
		else if (c == 10 || c == 13)
			cr_lf();
		else if (c == 127)
			del();
		else
			panic("panic: unsurpported char!\n");
	}
	write_char('_', x, y);
}
void panic(char *s)
{
	printk(s);
	while (1)
		;
}
void print_debug(char *str, unsigned long val)
{
	int i, j;
	char buffer[20];

	printk(str);
	buffer[0] = '0';
	buffer[1] = 'x';
	for (j = 0, i = 17; j < 16; j++, i--)
	{
		buffer[i] = (digits_map[val & 0xfUL]);
		val >>= 4;
	}
	buffer[18] = '\n';
	buffer[19] = '\0';
	printk(buffer);
}
void put_queue(char c)
{
	if (!c)
		return;
	if (read_queue.count == BUFFER_SIZE)
		return;
	read_queue.buffer[read_queue.head] = c;
	read_queue.head = (read_queue.head + 1) & (BUFFER_SIZE - 1);
	read_queue.count++;
	wake_up(&read_queue.wait);
}
int sys_output(char *buf)
{
	printk(buf);
	return 0;
}
int sys_input(char *buf)
{
	if (read_queue.count == 0)
		sleep_on(&read_queue.wait);
	*buf = read_queue.buffer[read_queue.tail];
	read_queue.tail = ((read_queue.tail) + 1) & (BUFFER_SIZE - 1);
	read_queue.count--;
	return 0;
}
void do_keyboard(unsigned char c)
{


	int row, col;
    
	if(c == 0x75 || c == 0x72 || c == 0x6b || c == 0x74)
	{
		if(isFirst!=1)
		{
        //写入原始数据save
        pos = (char *)(VRAM_BASE + (y * CHAR_HEIGHT * NR_PIX_X + x * CHAR_WIDTH) * NR_BYTE_PIX);
		for (row = 0; row < CHAR_HEIGHT; row++)
		{
			for (col = 0; col < CHAR_WIDTH; col++)
			{
				*pos=*temp;pos++;temp++;
				*pos=*temp;pos++;temp++;
				*pos=*temp;pos++;temp++;
				*pos=*temp;pos++;temp++;
			}
			pos += (NR_PIX_X - CHAR_WIDTH) * NR_BYTE_PIX;
			temp++;
		}	
		temp=save;	//temp归位
		}
		else
		{
        //是第一次则写入空，直接用temp可能会有上次残留的数据污染
		erase_char(x,y);
		isFirst=0;
		}
	}
	if(c == 0x75) // up arrow
	{
		if(y > 0)//只有y>0时才能往上移动光标，防止越界
			y--;
	}
	else if(c == 0x72) // down arrow
	{
		if(y < NR_CHAR_Y - 1)//只有y<NR_CHAR_Y-1时才能往下移动光标，防止越界
			y++;
	}
	else if(c == 0x6b) // left arrow
	{
		if(x > 0)//只有x>0时才能往左移动光标，防止越界
		{
			x--;//光标向左移动一格
		}else if(x == 0 && y>0)//换上一行
		{
            y--;
			x=NR_CHAR_X-1;
		}
	}	
	else if(c == 0x74) // right arrow
	{
		if(x < NR_CHAR_X - 1)//只有x<NR_CHAR_X-1时才能往右移动光标，防止越界
		{
			x++;//光标向右移动一格
		}else if(x == NR_CHAR_X -1 && y <NR_CHAR_Y -1)//换下一行
		{
			y++;
			x=0;
		}
	}
	else
	{
	if(def == 1)
		put_queue(keys_map_1[c]);
	else 
		put_queue(keys_map[c]);

	}
	//保存数据
	temp=save;
	pos = (char *)(VRAM_BASE + (y * CHAR_HEIGHT * NR_PIX_X + x * CHAR_WIDTH) * NR_BYTE_PIX);
	for (row = 0; row < CHAR_HEIGHT; row++)
	{
		for (col = 0; col < CHAR_WIDTH; col++)
		{
			*temp=*pos;temp++;pos++;
			*temp=*pos;temp++;pos++;
			*temp=*pos;temp++;pos++;
			*temp=*pos;temp++;pos++;
		}
		pos += (NR_PIX_X - CHAR_WIDTH) * NR_BYTE_PIX;
		temp++;
	} 
	temp=save;
	erase_char(x,y);
	write_char('_', x, y);//在光标处写入下划线
}
void keyboard_interrupt()
{
	unsigned char c;
	unsigned char b;
	unsigned char a;

	c = *(volatile unsigned char *)L7A_I8042_DATA;
	b = *(volatile unsigned char *)L7A_I8042_DATA;
	a = *(volatile unsigned char *)L7A_I8042_DATA;
	if (c == 0xf0)
	{
		 // 如果是松开前缀，读取后续的扫描码
        c = *(volatile unsigned char *)L7A_I8042_DATA;
        
        // 如果扫描码是0x12，表示 Shift 键松开
        if(c == 0x12 || c == 0x59)
        {
            def = 0; // 设置 `def` 为 0，表示没有按下 Shift 键
        }
        return;
	}
	
	if (c == 0x12 || c == 0x59)
    {
        def = 1; // 设置 `def` 为 1，表示 Shift 键按下
    }
	
	 if(c == 0xe0)
	{
		if(b == 0xf0)
		{
			return;
		}
        do_keyboard(a);//up:0x75 down:0x72 left:0x6b right:0x74
		return;
	}
	do_keyboard(c);
}
void con_init()
{
	read_queue.count = 0;
	read_queue.head = 0;
	read_queue.tail = 0;
	read_queue.wait = 0;
	read_queue.buffer = (char *)get_page();

	x = 0;
	y = 0;
}
void timer_interrupt()
{
	add--;
	
	if(add==0)
	{
	        if(flag==0)
		{
			write_char('_',x,y);
			flag=1;
		}
		else
		{
			erase_char(x,y);
			flag=0;
		}
		add=1;
	}
	
}

/*void print_picture()
{
	char *pos = (char *)(VRAM_BASE );
	for (int row = 0; row < 56; row++)
	{
		for (int col = 0; col < 58; col++)
		{
			*pos++ = buffer[row * 58 + col][0];
			*pos++ = buffer[row * 58 + col][1];
			*pos++ = buffer[row * 58 + col][2];
			*pos++ = 0;//透明度
		}
		pos += (NR_PIX_X - 58) * NR_BYTE_PIX;
	}
} */

void clear_screen()
{
    int i, j;
    // 遍历所有字符位置，逐个清空
    for (i = 0; i < NR_CHAR_Y; i++)
    {
        for (j = 0; j < NR_CHAR_X; j++)
        {
            erase_char(j, i);
        }
        sum_char_x[i] = 0; // 重置每行字符计数
    }
    // 重置光标到初始位置
    x = 0;
    y = 0;
    flag = 0; // 重置光标闪烁标志
    add = 1;  // 重置闪烁计数
    isFirst = 1; // 重置光标移动标志
}

// 打印由*组成的Welcome图案函数
void print_welcome()
{
    clear_screen();
    
    // 精准居中计算
    int screen_center_x = NR_CHAR_X / 2;
    int screen_center_y = NR_CHAR_Y / 2;
    int start_x = screen_center_x - 40;
    int start_y = screen_center_y - 10;

    // ========== 1. W（8行×8列，倒M形，标准W） ==========
    int w_x = start_x;
    for(int i=0; i<8; i++) { 
        write_char('*', w_x+0, start_y+i); 
        write_char('*', w_x+7, start_y+i); 
    }
    write_char('*', w_x+1, start_y+6); write_char('*', w_x+6, start_y+6);
    write_char('*', w_x+2, start_y+5); write_char('*', w_x+5, start_y+5);
    write_char('*', w_x+3, start_y+4); write_char('*', w_x+4, start_y+4);

    // 字母间隔：4列空白
    // ========== 2. E（8行×6列） ==========
    int e1_x = start_x + 12;
    for(int i=0; i<8; i++) write_char('*', e1_x+0, start_y+i);
    write_char('*', e1_x+1, start_y+0); write_char('*', e1_x+2, start_y+0); write_char('*', e1_x+3, start_y+0); write_char('*', e1_x+4, start_y+0);
    write_char('*', e1_x+1, start_y+3); write_char('*', e1_x+2, start_y+3); write_char('*', e1_x+3, start_y+3); write_char('*', e1_x+4, start_y+3);
    write_char('*', e1_x+1, start_y+7); write_char('*', e1_x+2, start_y+7); write_char('*', e1_x+3, start_y+7); write_char('*', e1_x+4, start_y+7);

    // 字母间隔：4列空白
    // ========== 3. L（8行×6列） ==========
    int l_x = start_x + 22;
    for(int i=0; i<8; i++) write_char('*', l_x+0, start_y+i);
    write_char('*', l_x+1, start_y+7); write_char('*', l_x+2, start_y+7); write_char('*', l_x+3, start_y+7); write_char('*', l_x+4, start_y+7);

    // 字母间隔：4列空白
    // ========== 4. C（8行×6列） ==========
    int c_x = start_x + 32;
    write_char('*', c_x+1, start_y+0); write_char('*', c_x+2, start_y+0); write_char('*', c_x+3, start_y+0); write_char('*', c_x+4, start_y+0);
    for(int i=1; i<7; i++) write_char('*', c_x+1, start_y+i);
    write_char('*', c_x+1, start_y+7); write_char('*', c_x+2, start_y+7); write_char('*', c_x+3, start_y+7); write_char('*', c_x+4, start_y+7);

    // 字母间隔：4列空白
    // ========== 5. O（8行×6列） ==========
    int o_x = start_x + 42;
    write_char('*', o_x+1, start_y+0); write_char('*', o_x+2, start_y+0); write_char('*', o_x+3, start_y+0); write_char('*', o_x+4, start_y+0);
    for(int i=1; i<7; i++) { write_char('*', o_x+1, start_y+i); write_char('*', o_x+4, start_y+i); }
    write_char('*', o_x+1, start_y+7); write_char('*', o_x+2, start_y+7); write_char('*', o_x+3, start_y+7); write_char('*', o_x+4, start_y+7);

    // 字母间隔：4列空白
    // ========== 6. M（8行×8列） ==========
    int m_x = start_x + 52;
    for(int i=0; i<8; i++) { write_char('*', m_x+0, start_y+i); write_char('*', m_x+7, start_y+i); }
    write_char('*', m_x+1, start_y+1); write_char('*', m_x+6, start_y+1);
    write_char('*', m_x+2, start_y+2); write_char('*', m_x+5, start_y+2);
    write_char('*', m_x+3, start_y+3); write_char('*', m_x+4, start_y+3);

    // 字母间隔：4列空白
    // ========== 7. E（第二个） ==========
    int e2_x = start_x + 64;
    for(int i=0; i<8; i++) write_char('*', e2_x+0, start_y+i);
    write_char('*', e2_x+1, start_y+0); write_char('*', e2_x+2, start_y+0); write_char('*', e2_x+3, start_y+0); write_char('*', e2_x+4, start_y+0);
    write_char('*', e2_x+1, start_y+3); write_char('*', e2_x+2, start_y+3); write_char('*', e2_x+3, start_y+3); write_char('*', e2_x+4, start_y+3);
    write_char('*', e2_x+1, start_y+7); write_char('*', e2_x+2, start_y+7); write_char('*', e2_x+3, start_y+7); write_char('*', e2_x+4, start_y+7);

    delay_ms(3000);
    clear_screen();
    x = 0; y = 0;
    write_char('_', x, y);
}

void print_lightenos()
{
    clear_screen();
    
    // 精准居中计算
    int screen_center_x = NR_CHAR_X / 2;
    int screen_center_y = NR_CHAR_Y / 2;
    int start_x = screen_center_x - 40;
    int start_y = screen_center_y - 10;


    // ========== 1. L ==========
    int l_x = start_x;
    for(int i=0; i<8; i++) write_char('*', l_x, start_y+i);  // 左竖线
    for(int i=0; i<5; i++) write_char('*', l_x+i, start_y+7); // 底部横线

    // 间隔4列
    // ========== 2. I ==========
    int i_x = start_x + 10;  // L(5列)+间隔(5列)=10
    for(int i=0; i<8; i++) write_char('*', i_x+2, start_y+i);  // 中间竖线
    for(int i=0; i<5; i++) write_char('*', i_x+i, start_y+0);   // 顶部横线
    for(int i=0; i<5; i++) write_char('*', i_x+i, start_y+7);   // 底部横线

    // 间隔4列
    // ========== 3. G（正确的G形） ==========
    int g_x = start_x + 20;  // I(5列)+间隔(5列)=20
    // 顶部横线
    for(int i=0; i<5; i++) write_char('*', g_x+i, start_y+0);
    // 左竖线
    for(int i=1; i<7; i++) write_char('*', g_x, start_y+i);
    // 底部横线
    for(int i=0; i<5; i++) write_char('*', g_x+i, start_y+7);
    // 右竖线（部分）
    for(int i=1; i<3; i++) write_char('*', g_x+4, start_y+i);
    // G的特殊横线（中间向右延伸）
    write_char('*', g_x+3, start_y+4);
    write_char('*', g_x+4, start_y+4);
    // 右竖线下半部分
    write_char('*', g_x+4, start_y+5);
    write_char('*', g_x+4, start_y+6);

    // 间隔4列
    // ========== 4. H ==========
    int h_x = start_x + 30;  // G(5列)+间隔(5列)=30
    // 左竖线
    for(int i=0; i<8; i++) write_char('*', h_x, start_y+i);
    // 右竖线
    for(int i=0; i<8; i++) write_char('*', h_x+4, start_y+i);
    // 中间横线
    for(int i=0; i<5; i++) write_char('*', h_x+i, start_y+3);

    // 间隔4列
    // ========== 5. T ==========
    int t_x = start_x + 40;  // H(5列)+间隔(5列)=40
    // 顶部横线
    for(int i=0; i<5; i++) write_char('*', t_x+i, start_y+0);
    // 中间竖线
    for(int i=1; i<8; i++) write_char('*', t_x+2, start_y+i);

    // 间隔4列
    // ========== 6. E ==========
    int e_x = start_x + 50;  // T(5列)+间隔(5列)=50
    // 左竖线
    for(int i=0; i<8; i++) write_char('*', e_x, start_y+i);
    // 顶部横线
    for(int i=0; i<5; i++) write_char('*', e_x+i, start_y+0);
    // 中间横线
    for(int i=0; i<4; i++) write_char('*', e_x+i+1, start_y+3);
    // 底部横线
    for(int i=0; i<5; i++) write_char('*', e_x+i, start_y+7);

    // 间隔4列
    // ========== 7. N（正确的N形） ==========
    int n_x = start_x + 60;  // E(5列)+间隔(5列)=60
    // 左竖线
    for(int i=0; i<8; i++) write_char('*', n_x, start_y+i);
    // 右竖线
    for(int i=0; i<8; i++) write_char('*', n_x+4, start_y+i);
    // 对角线（从左上到右下）
    write_char('*', n_x+1, start_y+1);
    write_char('*', n_x+2, start_y+2);
    write_char('*', n_x+3, start_y+3);

    // 间隔4列
    // ========== 8. O ==========
    int o_x = start_x + 70;  // N(5列)+间隔(5列)=70
    // 顶部横线
    for(int i=0; i<5; i++) write_char('*', o_x+i, start_y+0);
    // 底部横线
    for(int i=0; i<5; i++) write_char('*', o_x+i, start_y+7);
    // 左竖线
    for(int i=1; i<7; i++) write_char('*', o_x, start_y+i);
    // 右竖线
    for(int i=1; i<7; i++) write_char('*', o_x+4, start_y+i);

    // 间隔4列
    // ========== 9. S（正确的S形） ==========
    int s_x = start_x + 80;  // O(5列)+间隔(5列)=80
    // 顶部横线
    for(int i=0; i<5; i++) write_char('*', s_x+i, start_y+0);
    // 左上角竖线
    write_char('*', s_x, start_y+1);
    write_char('*', s_x, start_y+2);
    // 中间横线
    for(int i=0; i<5; i++) write_char('*', s_x+i, start_y+3);
    // 右下角竖线
    write_char('*', s_x+4, start_y+4);
    write_char('*', s_x+4, start_y+5);
	write_char('*', s_x+4, start_y+6);
    // 底部横线
    for(int i=0; i<5; i++) write_char('*', s_x+i, start_y+7);
    // 左下角竖线（下半部分）
    write_char('*', s_x, start_y+5);
	write_char('*', s_x, start_y+6);
    // 右上角竖线（上半部分）
    write_char('*', s_x+4, start_y+1);

    // 延迟显示
    delay_ms(3000);
    clear_screen();
    x = 0; y = 0;
    write_char('_', x, y);
}
