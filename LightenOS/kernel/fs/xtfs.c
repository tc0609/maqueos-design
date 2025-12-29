#include <xtos.h>

#define NR_INODE BLOCK_SIZE / sizeof(struct inode)

struct inode inode_table[NR_INODE];
char block_map[BLOCK_SIZE];
extern struct process *current;
extern struct process *process[NR_PROCESS];

short get_block()
{
	short blocknr;
	int i, j;

	for (i = 0; i < BLOCK_SIZE; i++)
	{
		if (block_map[i] == 255)
			continue;
		for (j = 0; j < 8; j++)
		{
			if ((block_map[i] & (1 << j)) != 0)
				continue;
			block_map[i] |= 1 << j;
			blocknr = i * 8 + j;
			clear_block(blocknr);
			return blocknr;
		}
	}
	panic("panic: block_map[] is empty!\n");
	return 0;
}
void put_block(short blocknr)
{
	int i, j;

	i = blocknr / 8;
	j = blocknr % 8;
	block_map[i] &= ~(1 << j);
	free_block(blocknr);
}
struct inode *find_inode(char *filename)
{
	int i;

	for (i = 0; i < NR_INODE; i++)
	{
		if (inode_table[i].type == 0)
			continue;
		if (match(filename, inode_table[i].filename, NAME_LEN))
			return &inode_table[i];
	}
	return 0;
}
void read_inode_block(struct inode *inode, short file_blocknr, char *buf, int size)
{
	char *block;
	short blocknr;
	short *index_table;

	index_table = (short *)read_block(inode->index_table_blocknr);
	blocknr = index_table[file_blocknr];
	block = read_block(blocknr);
	copy_mem(buf, block, size);
}
// int sys_read(int fd, char *buf)
// {
// 	struct file *file;
// 	short file_blocknr;

// 	file = &current->file_table[fd];
// 	file_blocknr = file->pos_r++;
// 	read_inode_block(file->inode, file_blocknr, buf, BLOCK_SIZE);
// 	return 0;
// }

int sys_read(int fd, char *buf)
{
    struct file *file;
    struct inode *inode;          // 缓存inode指针，减少冗余寻址
    short *index_table;           // 文件索引表指针，仅读取一次
    int blockCount;               // 总共需要读取的磁盘块数
    int total_read_bytes = 0;     // 实际读取的总字节数
    int remain_bytes;             // 最后一个块的实际读取字节数
    int read_bytes_per_block;     // 每个块的实际读取字节数
    int i;
    short file_blocknr;           // 当前读取的文件块索引
    short disk_blocknr;           // 当前文件块对应的磁盘块编号
    const int MAX_BLOCK_CNT = 256; // 索引表最大支持256个块（和sys_write保持一致）

    // 多层边界防护——校验fd合法性
    if (fd < 0 || fd >= NR_FILE)
    {
        printk("sys_read: invalid file descriptor\n");
        return -1; // -1：非法文件描述符
    }

    
    if (buf == 0)
    {
        printk("sys_read: buf is null\n");
        return -2; // -2：缓冲区为空
    }

    // 获取文件表项
    file = &current->file_table[fd];
    inode = file->inode;

    //多层边界防护——校验inode非空（文件未打开）
    if (inode == 0)
    {
        printk("sys_read: inode is null (file not opened)\n");
        return -3; // -3：文件inode为空（未成功打开文件）
    }

    //校验文件大小——文件为空时直接返回0
    if (inode->size <= 0)
    {
        return 0; // 无数据可读，返回0
    }

    //越界校验——校验读取指针pos_r，避免索引表溢出
    if (file->pos_r >= MAX_BLOCK_CNT)
    {
        printk("sys_read: read pointer out of bounds (pos_r too large)\n");
        return -4; // -4：读取指针越界
    }

    //减少冗余磁盘IO——仅读取一次索引表（移到循环外）
    index_table = (short *)read_block(inode->index_table_blocknr);
   
    if (index_table == 0)
    {
        printk("sys_read: failed to read index table\n");
        return -5; // -5：索引表读取失败
    }

    // 计算总共需要读取的磁盘块数（基于文件剩余大小）
    int remaining_file_size = inode->size - (file->pos_r * BLOCK_SIZE);
    if (remaining_file_size <= 0)
    {
        return 0; // 已读取到文件末尾，无剩余数据
    }
    blockCount = (remaining_file_size % BLOCK_SIZE == 0) ? 
                 (remaining_file_size / BLOCK_SIZE) : 
                 (remaining_file_size / BLOCK_SIZE + 1);

    // 再次校验：避免读取块数超出索引表剩余容量
    if ((file->pos_r + blockCount) > MAX_BLOCK_CNT)
    {
        printk("sys_read: no enough index table space for reading\n");
        return -4; // -4：索引表容量不足
    }

    // 循环读取每个磁盘块
    for (i = 0; i < blockCount; i++)
    {
        // 获取当前读取的文件块索引（基于pos_r偏移）
        file_blocknr = file->pos_r++;
        // 获取当前文件块对应的磁盘块编号
        disk_blocknr = index_table[file_blocknr];

        // 校验磁盘块是否已分配
        if (disk_blocknr == 0)
        {
            // 移除格式化输出，用intToString转数字为字符串后分多次输出
            char block_num_str[12]; // 适配intToString的缓冲区大小
            printk("sys_read: disk block not allocated (blocknr: ");
            intToString(file_blocknr, block_num_str); // 数字转字符串
            printk(block_num_str);
            printk("\n");
            break; // 磁盘块未分配，终止读取
        }

        //精准计算每个块的实际读取字节数
        if (i == blockCount - 1 && (remaining_file_size % BLOCK_SIZE != 0))
        {
            // 最后一个块：读取剩余有效字节数
            remain_bytes = remaining_file_size % BLOCK_SIZE;
            read_bytes_per_block = remain_bytes;
        }
        else
        {
            // 完整块：读取BLOCK_SIZE字节
            read_bytes_per_block = BLOCK_SIZE;
        }

        // 读取磁盘块数据到用户缓冲区
        read_inode_block(inode, file_blocknr, buf + i * BLOCK_SIZE, read_bytes_per_block);
        // 累加实际读取字节数
        total_read_bytes += read_bytes_per_block;
    }

    //返回实际读取的总字节数（非固定0）
    return total_read_bytes;
}
/**
 * sys_write - 内核态文件写入系统调用
 * @fd：文件描述符（0 ~ NR_FILE-1，NR_FILE=10）
 * @buf：用户缓冲区地址，存储要写入的字符串（以\0结尾）
 * 返回值：>=0=实际写入的字节数；<0=错误码（-1=非法fd，-2=inode为空，-3=buf为空，-4=写指针越界）
 */
int sys_write(int fd, char *buf)
{
    struct file *file;
    struct inode *inode; // 缓存inode指针，减少冗余寻址
    short file_blocknr;
    short blocknr;
    short *index_table;
    char *p;
    int buf_len = 0;
    int blockCount;
    int i; 
    int total_write_bytes = 0; // 实际写入的总字节数
    int remain_bytes; // 最后一个块的实际写入字节数
    const int MAX_WRITE_LEN = BLOCK_SIZE * 256; // 最大支持写入长度（对应索引表256个块）

    //多层边界防护——校验fd合法性
    if (fd < 0 || fd >= NR_FILE)
    {
        printk("sys_write: invalid file descriptor\n");
        return -1;
    }

    //多层边界防护——校验buf非空
    if (buf == 0)
    {
        printk("sys_write: buf is empty\n");
        return -3;
    }

    // 获取文件表项
    file = &current->file_table[fd];
    //多层边界防护——校验inode非空（用0代替NULL，内核兼容）
    inode = file->inode;
    if (inode == 0)
    {
        printk("sys_write: inode is empty\n");
        return -2;
    }

    //安全计算buf长度——防止无\0导致无限循环
    p = buf;
    while (*p != '\0' && buf_len < MAX_WRITE_LEN)
    {
        buf_len++;
        p++;
    }
    // 若buf长度为0，直接返回
    if (buf_len == 0)
    {
        return 0;
    }

    //校验写指针越界——避免索引表溢出（索引表最大256个块）
    if (file->pos_w >= 256)
    {
        printk("sys_write: write pointer out of bounds\n");
        return -4;
    }

    // 计算需要的磁盘块总数
    blockCount = (buf_len % BLOCK_SIZE == 0) ? (buf_len / BLOCK_SIZE) : (buf_len / BLOCK_SIZE + 1);
    // 再次校验：避免块数超过索引表剩余容量
    if ((file->pos_w + blockCount) > 256)
    {
        printk("sys_write: no enough index table space\n");
        return -4;
    }

    //减少冗余磁盘IO——仅读取一次索引表（移到循环外）
    index_table = (short *)read_block(inode->index_table_blocknr);

    // 循环写入每个磁盘块
    for (i = 0; i < blockCount; i++)
    {
        // 获取当前写入的文件块索引，并更新写指针
        file_blocknr = file->pos_w++;
        // 获取当前文件块对应的磁盘块编号
        blocknr = index_table[file_blocknr];

        // 若磁盘块未分配，分配新磁盘块
        if (blocknr == 0)
        {
            blocknr = get_block();
            index_table[file_blocknr] = blocknr; // 更新索引表
        }

        //计算实际写入字节数——避免文件大小虚高
        if (i == blockCount - 1 && (buf_len % BLOCK_SIZE != 0))
        {
            // 最后一个块：写入剩余有效字节数
            remain_bytes = buf_len % BLOCK_SIZE;
            write_block(blocknr, buf + i * BLOCK_SIZE);
            total_write_bytes += remain_bytes;
            inode->size += remain_bytes; // 累加实际写入字节数
        }
        else
        {
            // 完整块：写入BLOCK_SIZE字节
            write_block(blocknr, buf + i * BLOCK_SIZE);
            total_write_bytes += BLOCK_SIZE;
            inode->size += BLOCK_SIZE; // 累加完整块大小
        }
    }

    //返回实际写入字节数（而非固定0）
    return total_write_bytes;
}
int sys_create(char *filename)
{
	int i;

	if (find_inode(filename))
		return -1;
	for (i = 0; i < NR_INODE; i++)
		if (inode_table[i].type == 0)
			break;
	if (i == NR_INODE)
		panic("panic: inode_table[] is empty!\n");
	inode_table[i].type = 2;
	inode_table[i].index_table_blocknr = get_block();
	inode_table[i].size = 0;
	copy_string(inode_table[i].filename, filename);
	return 0;
}
int sys_destroy(char *filename)
{
	int i, j;
	struct inode *inode;
	short *index_table;

	inode = find_inode(filename);
	if (!inode)
		return -1;
	for (i = 1; i < NR_PROCESS; i++)
	{
		if (process[i] == 0)
			continue;
		if (process[i]->executable == inode)
			panic("panic: can not destroy opened executable!\n");
		for (j = 0; j < NR_FILE; j++)
		{
			if (process[i]->file_table[j].inode == inode)
				panic("panic: can not destroy opened file!\n");
		}
	}
	index_table = (short *)read_block(inode->index_table_blocknr);
	for (i = 0; i < 256; i++)
	{
		if (index_table[i] == 0)
			break;
		put_block(index_table[i]);
	}
	put_block(inode->index_table_blocknr);
	inode->type = 0;
	return 0;
}
// new
void intToString(int num, char* str) {

    int i = 0;
    do {
        str[i++] = (num % 10) + '0'; // 提取最低位并转换为字符
        num /= 10;                    // 去掉最低位
    } while (num > 0);
    // 添加字符串结束符
    str[i] = '\0';

    for (int j = 0; j < i / 2; j++) {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }
}
//new
int sys_file_size(char *filename)
{
    struct inode *inode;
    char result[12]; // 适配intToString函数，足够存储整数转字符串的结果（含\0）

    //校验filename非空，避免无效参数
    if (filename == 0)
    {
        printk("sys_file_size: filename is null\n");
        return -1; // -1：文件名参数为空
    }

    // 查找对应inode
    inode = find_inode(filename);

    //校验inode非空，避免访问无效内存（文件不存在）
    if (inode == 0)
    {
        printk("sys_file_size: file not found\n");
        return -2; // -2：文件不存在
    }

    // 将文件大小转为字符串并打印
    intToString(inode->size, result);
    printk("File size: "); // 提示信息
    printk(result);
    printk("\n");

    //成功返回文件大小（用户态可获取该数值）
    return inode->size;
}
// new
int sys_filetype(char *filename)
{
    struct inode *inode;

    //校验filename非空，避免无效参数
    if (filename == 0)
    {
        printk("sys_filetype: filename is null\n");
        return -1; // -1：文件名参数为空
    }

    // 查找对应inode
    inode = find_inode(filename);

    //校验inode非空，提示文件不存在
    if (inode == 0)
    {
        printk("sys_filetype: file not found\n");
        return -2; // -2：文件不存在
    }

    //增强打印直观性，关联文件名和文件类型
    printk("File: ");
    printk(filename);
    printk(" -> ");

    //规整条件判断
    switch (inode->type)
    {
        case 1:
            printk("executable file\n");
            break;
        case 2:
            printk("read and write file\n");
            break;
        case 3:
            printk("only read file\n");
            break;
        case 4:
            printk("only write file\n");
            break;
        default:
            printk("unknown type file\n");
            break;
    }

    //成功返回文件类型实际数值，方便用户态使用
    return inode->type;
}
int sys_open(char *filename)
{
	int i;
	struct inode *inode;

	inode = find_inode(filename);
	if (!inode)
		return -1;
		// new
	if (inode->type !=2 &&  inode->type !=3  &&  inode->type != 4)
		panic("panic: wrong type!\n");
	for (i = 0; i < NR_FILE; i++)
		if (current->file_table[i].inode == 0)
			break;
	if (i == NR_FILE)
		panic("panic: current->file_table[] is empty!\n");
	current->file_table[i].inode = inode;
	current->file_table[i].pos_r = 0;
	current->file_table[i].pos_w = 0;
	return i;
}
int sys_close(int i)
{
	current->file_table[i].inode = 0;
	current->file_table[i].pos_r = 0;
	current->file_table[i].pos_w = 0;
	return 0;
}
int sys_mount()
{
	char *block;

	block = read_block(0);
	copy_mem((char *)inode_table, block, BLOCK_SIZE);
	block = read_block(1);
	copy_mem(block_map, block, BLOCK_SIZE);
	return 0;
}
// #new
int sys_ls(void)
{
    int i;

    // 边界防护：错误提示
    if (NR_INODE <= 0 || NR_INODE > (BLOCK_SIZE / sizeof(struct inode)))
    {
        printk("ls: invalid NR_INODE value\n"); // 仅打印固定错误字符串，无数值
        return -1;
    }

    // 打印提示信息
    printk("Files in system:\n");

    // 遍历inode表，打印有效文件名
    for (i = 0; i < NR_INODE; i++)
    {
        if (inode_table[i].type == 0)
            continue;
        if (!inode_table[i].filename[0])
            continue;

        printk("  ");
        printk(inode_table[i].filename);
        printk("\n");
    }

    printk("ls: list files completed\n");
    return 0;
}
void close_files()
{
	int i;

	for (i = 0; i < NR_FILE; i++)
	{
		if (current->file_table[i].inode)
			sys_close(i);
	}
}
void write_first_two_blocks()
{
	write_block(0, (char *)inode_table);
	write_block(1, block_map);
}
