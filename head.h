#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <direct.h>


#define MAX_BLOCKS_NUM 256       //最大数据块数量是256，则数据块总大小是256KB
#define MAX_BLOCK_SIZE 1025       //数据块数据容量,一个数据块的大小是1KB,最后1位用来存储'\0',表示字符串结尾！
#define MAX_DATANODE_NUM 2       //每个文件最多占用的数据块的数量
#define MAX_INODE_NUM  512       //i节点的最大数目，亦即系统允许容纳文件的最大数量
#define MAX_CACHE_NUM 32          //允许缓存的最大数据块数量为32个数据块，缓存为32KB



/*---------------结构定义-----------------*/

/*---------用户--------*/
typedef struct user{
	char account[15];                    //账户最长为10
	char password[15];                   //密码最长为10,多出来的是为了方便运算
}user;


/*--------数据节点--------*/
typedef struct datanode{         //用于记录文件数据保存在哪个数据块的哪个范围
	int num;                  //数据块号
	int begin;                //数据开始位置
	int end;                  //数据结束位置
}datanode;

/*-------i节点--------*/
typedef struct inode{
	char filename[30];  //文件名
	int num;            //当前i节点数量，亦即文件数量
	char code[30];      //保护码
	int size;           //文件大小
	datanode fat[MAX_DATANODE_NUM]; //所占用数据块内部的起始信息
	int node_num;       //占用的数据块数
}inode;

/*-------inode链表-------*/
typedef struct dirEntry{         //用链表来记录I节点
	inode ind;            //Inode结点
	struct dirEntry *next;
}dirEntry;



/*-------数据块--------*/
typedef struct block{
	char content[MAX_BLOCK_SIZE];           //数据块内容最大长度为1024
	int num;                                //数据块id
	bool isRevised;                         //用于记录数据块是否进行数据修改
	int offset;                             //记录当前数据的数量
}block;




/*-------------定义全局变量-------------------*/
int islogin = 0;
char blockspath[30] = "userdata/blocksDISK.disk";  //  所有数据(即虚拟磁盘)地址
char userspath[30] = "userdata/users.us";
static user currUser;             //当前用户
static dirEntry *DIR;
static int curr_inode_num = 0;                //当前i节点数量，亦即文件数量
int max_node = 0;            //最大的inode编号
static int FBT[MAX_BLOCKS_NUM];
static char *dirpath;
static char fbtpath[30] = "userdata/FBT.disk";
static dirEntry* selectEntry;      //当selectEntry==NULL时，证明没有打开文件
static dirEntry* currEntry;



//命令行
static char cm_help[10] = "help";
static char cm_lg[10] = "login";
static char cm_rg[10] = "register";
static char cm_dir[10] = "dir";
static char cm_create[10] = "create";
static char cm_delete[10] = "delete";
static char cm_open[10] = "open";
static char cm_read[10] = "read";
static char cm_write[10] = "write";
static char cm_close[10] = "close";
static char cm_exit[10] = "exit";
static char cm_cancle[10] = "cancle";




/*-------------函数声明---------------*/
void start();                   //启动函数
void getUser();                 //获取当前登录用户信息
int login();                    //登录
int regist();                   //注册
char lgOrRg();                  //判断进行登录操作或注册操作

int getAction();                //获取行为码
int getCode();                  //获取文件相关命令码

void saveFBT();                 //保存数据块状态信息
void saveDir();                 //保存文件

void createDataDir();           //创建用户目录
void createBlocksDisk();        //创建磁盘块，初始数据为"$$$.."，FBT清0，表示未被使用
FILE* createDir();              //创建文件
void initDir(char *);           //初始化文件
void initFBT();                  //初始化数据空闲块记录表
int getFreeBlock(bool);         //申请空闲块
void saveBlock(block bk);       //保存数据块

dirEntry* delHelp(char *filename);





//响应指令的函数
int LgRg();
void help();
void dir();
void create();
void del();
void open();
void close();
void read();
void write();
void coverHelp();
void appendHelp();


int strcmpi(char *p1, char *p2){
	if(strlen(p1)!=strlen(p2))
	{
		return -1;
	}
	int i;
	for(i = 0; i < strlen(p1); i++)
	{
		if(p1[i]!=p2[i])
		{
			return -1;
		}
	}
	return 0;
}

/**
 * 创建用户目录
 */
void createDataDir(){
	char *datapath = "userdata";
	if(_mkdir(datapath)){
		printf("Can not create DataDir ...\nPlease contact QQxxxx for help...\nCrtl+C to exit.\n");
		while(1);
	}
}


/**
 * 判断进行登录操作或注册操作
 * @return 若是登录操作，返回'l'；若是注册操作，返回'r'
 */
char lgOrRg(){
	while(1){
		char com[10];
		printf("[login] or [register]: ");
		scanf("%s", com);
		if(!strcmp(com, cm_lg))
			return 'l';
		if(!strcmp(com, cm_rg))
			return 'r';
	}
}

void help(){
	printf("欢迎您使用FMS文件系统模拟系统 V1.0");
	printf("以下是本系统支持的指令:\n");
	printf("exit : 退出\n");
	printf("help : 帮助\n");
	printf("dir : 查看目录中的所有文件\n");
	printf("create : 新建文件\n");
	printf("delete : 删除文件\n");
	printf("open : 打开文件\n");
	printf("read : 读文件（必须先打开文件）\n");
	printf("write : 写文件（必须先打开文件）\n");
	printf("close : 关闭文件\n");
}

/**
 * 创建磁盘块，初始数据为"$$$.."，FBT清0，表示未被使用
 */
void createBlocksDisk(){
	FILE* fp = fopen(blockspath, "w");
	if(NULL == fp){
		printf("Can not create Disk !\nCtrl+C to quit\n");
		while(1);
	}
	else{
		for(int i = 0; i < MAX_BLOCKS_NUM; i++){
			for(int j = 0; j < MAX_BLOCK_SIZE; j++){
				fputc('$',fp);
			}
		}
		fclose(fp);
	}
	FILE *p = fopen(fbtpath, "w");
	if(NULL == p){
		printf("FBT Created ERROR !\nCtrl+C to quit\n");
		while(1);
	}
	else{
		for(int i = 0; i < MAX_BLOCKS_NUM; i++){
			FBT[i] = 0;
			fprintf(p, " %d", 0);
		}
		fclose(p);
	}
}

/**
 * 判断是否有重名文件
 * @param filename 文件名
 * @return 若重名，返回其文件指针，否则返回NULL
 */
dirEntry* isInDir(char *filename){
	dirEntry *pt = DIR;
	while(pt != NULL){
		if(!strcmpi(pt->ind.filename, filename))
			return pt;
		pt = pt->next;
	}
	return NULL;
}

/**
 * 删除文件帮助函数
 * @param filename 文件名
 * @return 若有文件，删除并返回要删除的文件结点，否则返回NULL
 */
dirEntry* delHelp(char* filename){
	dirEntry* res = DIR;
	if(res == NULL){
		printf("No files !\n");
		return res;
	}
	if(res->next == NULL){
		if(!strcmpi(res->ind.filename, filename)){
			DIR = NULL;
			currEntry=NULL;
			printf("删除成功!\n");
			return res;
		}
		else
			return NULL;
	}
	if(!strcmpi(res->ind.filename, filename)){
		DIR = res->next;
		printf("删除成功!\n");
		return res;
	}
	while(res->next != NULL){
		if(!strcmpi(res->next->ind.filename, filename)){
			dirEntry* r = res->next;
			res->next = r->next;
			printf("删除成功!\n");
			return r;
		}
		res = res->next;
	}
	printf("删除失败!\n");
	return NULL;
}

/**
 * 覆盖写
 */
void coverHelp(){
	//释放原数据块，更新FBT
	int f;
	if(selectEntry->ind.size != 0){
		for(f = 0; f < selectEntry->ind.node_num; f++)
			FBT[selectEntry->ind.fat[f].num] = 0;
	}
	char content[MAX_DATANODE_NUM][MAX_BLOCK_SIZE];
	char tmp;
	inode ind;
	printf("End with '$'\n");
	int i = 0, index = 0;
	while((tmp=getchar()) != '$'){
		if(i == 0 && tmp == '\n')
			continue;
        if(index > MAX_BLOCK_SIZE - 1) index = 0;
		content[i/MAX_BLOCK_SIZE][index++] = tmp;
		i++;
	}
	ind.size = i;
	//此时已经结束输入
	if(i>(MAX_BLOCK_SIZE-1)*MAX_DATANODE_NUM){
		printf("文件过大，无法存储，创建失败 !\n");
		return;
	}
	int k;
	for(k = 0; k <= i/(MAX_BLOCK_SIZE-1); k++){
		block bk;
		int bkn;
		for(bkn = 0; bkn < MAX_BLOCK_SIZE-1; bkn++)
			bk.content[bkn] = '$';
		bk.content[MAX_BLOCK_SIZE-1] = '\0';

		int tp = 0;
		int len = 0;
		if(k == 0){
			if(i < MAX_BLOCK_SIZE-1)
				len = i;
		}
		if(k == 1)
			len = i % (MAX_BLOCK_SIZE - 1) + 1;
		for(tp = 0; tp < len; tp++)
			bk.content[tp] = content[k][tp];
		bk.isRevised = true;
		if(k == 0)
			bk.num = getFreeBlock(false);
		else{
			bk.num = getFreeBlock(true);
			if(bk.num==-1){
				printf("数据块已用完,内存不足!\n");
				return;
			}
		}
		saveBlock(bk);
		ind.fat[k].num = bk.num;
		ind.fat[k].begin = 0;
		ind.fat[k].end = len;
	}
	ind.node_num = k;
	strcpy(ind.code, selectEntry->ind.code);
	strcpy(ind.filename, selectEntry->ind.filename);
	ind.num = selectEntry->ind.node_num;
	selectEntry->ind = ind;
	saveDir();
	saveFBT();
	printf("文件已保存 !\n");
}

/**
 * 附加写
 */
void appendHelp(){
    char tmp[MAX_BLOCK_SIZE*2];
	char ch;
	printf("End with '$':\n");
	int i = 0;
	while((ch=getchar()) != '$'){
		if(i == 0 && ch == '\n')
			continue;
		tmp[i++] = ch;
	}
	tmp[i] = '\0';
	//此时已经完成输入
	if((i+selectEntry->ind.size) > (MAX_BLOCK_SIZE-1)*MAX_DATANODE_NUM){
		printf("文件过大，无法存储，创建失败 !\n");
		return;
	}
	else{
		if(selectEntry->ind.size > MAX_BLOCK_SIZE-1){               //已经占用了两个block
			int offset = selectEntry->ind.size - MAX_BLOCK_SIZE + 1;
			FILE* bfp = fopen(blockspath,"r+");
			if(bfp==NULL){
				printf("DISK ERROR !\nFrom appendFile ...\n");
				return;
			}
			else{
				fseek(bfp, (selectEntry->ind.fat[1].num*(MAX_BLOCK_SIZE-1) + offset), SEEK_SET);
				fwrite(tmp,sizeof(char),i,bfp);
				fclose(bfp);
				selectEntry->ind.size = selectEntry->ind.size + i;
				selectEntry->ind.fat[1].end = selectEntry->ind.fat[1].end + i;
				saveDir();
				printf("文件保存成功 !\n");
			}
		}
		else{            //只占用了一个block
			if(i < (MAX_BLOCK_SIZE - 1 - selectEntry->ind.size)){ //不会占用新的block
				FILE* bfp = fopen(blockspath,"r+");
				if(bfp==NULL){
					printf("DISK ERROR !\nFrom appendFile ...\n");
					return;
				}
				else{
					fseek(bfp,(selectEntry->ind.fat[0].num*(MAX_BLOCK_SIZE-1)+selectEntry->ind.size),SEEK_SET);
					fwrite(tmp,sizeof(char),i,bfp);
					fclose(bfp);
					selectEntry->ind.size = selectEntry->ind.size + i;
					selectEntry->ind.fat[0].end = selectEntry->ind.fat[0].end + i;
					saveDir();
					printf("文件保存成功 !\n");
				}
			}
			else{        //要占用新的block
				int bkNum = getFreeBlock(true);
				if(bkNum == -1){
					printf("数据块已用完,内存不足!\n");
					return;
				}
				char *p1 = (char*)malloc((MAX_BLOCK_SIZE-1-selectEntry->ind.size)*sizeof(char));
				char *p2 = (char*)malloc((i-(MAX_BLOCK_SIZE-1-selectEntry->ind.size))*sizeof(char));
				int pi;
				int pn1 = 0, pn2 = 0;
				for(pi = 0; pi < i; pi++){
					if(pi < MAX_BLOCK_SIZE-1-selectEntry->ind.size)
						p1[pn1++] = tmp[pi];
					else
						p2[pn2++] = tmp[pi];
				}
				p1[pn1] = '\0';
				p2[pn2] = '\0';
				//存储
				FILE *bfp = fopen(blockspath, "r+");
				if(bfp == NULL){
					printf("DISK ERROR !\nFrom appendFile ...\n");
					return;
				}
				else{
					fseek(bfp,((MAX_BLOCK_SIZE-1)*selectEntry->ind.fat[0].num+selectEntry->ind.fat[0].end),SEEK_SET);
					fwrite(p1,sizeof(char),pn1,bfp);
					fseek(bfp,((MAX_BLOCK_SIZE-1)*bkNum),SEEK_SET);
					fwrite(p2,sizeof(char),pn2,bfp);
					fclose(bfp);
					FBT[bkNum] = 1;
					selectEntry->ind.node_num = 2;
					selectEntry->ind.size = selectEntry->ind.size + i;
					selectEntry->ind.fat[0].end = MAX_BLOCK_SIZE - 2;
					selectEntry->ind.fat[1].num = bkNum;
					selectEntry->ind.fat[1].begin = 0;
					selectEntry->ind.fat[1].end = pn2;
					saveFBT();
					saveDir();
					printf("文件保存成功 !\n");
				}
			}
		}
	}
}


//核心函数

/**
 * 创建文件
 */
void create(){
	int bkNum = getFreeBlock(false);
	if(bkNum == -1){
		printf("数据块已用完,内存不足!\n");
		return;
	}
	char tmp;
	dirEntry *pt = (dirEntry*)malloc(sizeof(dirEntry));
	pt->next = NULL;
	while(1){
		printf("filename: ");
		scanf("%s", pt->ind.filename);
		if(isInDir(pt->ind.filename) != NULL)
			printf("文件名已存在 !\n请重新输入:\n");
		else
			break;
	}
	while(1){
		printf("Do you want to write the file ?[y/n]: ");
		scanf(" %c", &tmp);
		if((tmp == 'y')||(tmp == 'Y')||(tmp == 'n')||(tmp == 'N'))
			break;
	}

	pt->ind.num = curr_inode_num++;
	char code[10] = "rwxr_xr_x";
	strcpy(pt->ind.code, code);
	pt->ind.size = 0;
	pt->ind.node_num = 0;           //----------------------------------------------------------
	//初始化，跟存储要相符合
	if(tmp=='y'||tmp=='Y'){
		char content[MAX_DATANODE_NUM][MAX_BLOCK_SIZE];
		char tmp;
		printf("End with '$'\n");
		int i = 0, index = 0;
		while((tmp = getchar())!='$'){
			if(i == 0 && tmp == '\n')
				continue;
            if(index >= MAX_BLOCK_SIZE - 1) index = 0;
			content[i/(MAX_BLOCK_SIZE-1)][index++] = tmp;       //--------------------------------
			i++;
		}
		pt->ind.size = i;
		//此时已经结束输入
		if(i > (MAX_BLOCK_SIZE-1)*MAX_DATANODE_NUM){
			printf("文件过大，无法存储，创建失败 !\n");
			return;
		}
		int k;
		for(k = 0; k <= i/(MAX_BLOCK_SIZE - 1);k++){
			block bk;
			int bkn;
			for(bkn = 0; bkn < MAX_BLOCK_SIZE-1; bkn++){
				bk.content[bkn] = '$';
			}
			bk.content[MAX_BLOCK_SIZE-1] = '\0';
			//printf("bk.content: %s\n", bk.content);
			int tp = 0;
			int len = 0;
			//第一个数据块
			if(k == 0){
				if(i < MAX_BLOCK_SIZE - 1){
					len = i;
				}
			}
			//第二个数据块
			if(k == 1){
				len = i % (MAX_BLOCK_SIZE - 1) + 1;
			}
			for(tp = 0; tp < len; tp++){
				bk.content[tp] = content[k][tp];
			}
			bk.isRevised = true;
			if(k == 0){
				bk.num = bkNum;
			}
			else{
				bk.num = getFreeBlock(true);
				if(bk.num == -1){
					printf("数据块已用完,内存不足!\n");
					return;
				}
			}
			saveBlock(bk);                  //------------------------------------
			pt->ind.fat[k].num = bk.num;
			pt->ind.fat[k].begin = 0;
			pt->ind.fat[k].end = len;
		}
		pt->ind.node_num = k;
	}
	if(currEntry == NULL)
		DIR = pt;
	else{
		currEntry->next = pt;
	}
	currEntry = pt;
	saveDir();
	saveFBT();
	printf("Succeed create file [%s] !\n", pt->ind.filename);
}

/**
 * 删除文件
 */
void del(){
	char tmp[30];
	printf("请输入要删除的文件名: ");
	scanf("%s", tmp);
	if(isInDir(tmp) == NULL){
		printf("不存在这个文件。\n");
		return;
	}
	else{
		dirEntry *dle = delHelp(tmp);
		if(dle != NULL){
			int i;
			for(i = 0; i < dle->ind.node_num; i++)
				FBT[dle->ind.fat[i].num] = 0;
            free(dle);
		}
	}
	saveDir();
	saveFBT();
}

/**
 * 打开文件
 */
void open(){
	char file[50];
	printf("请输入文件名: ");
	scanf("%s", file);
	selectEntry = isInDir(file);
	if(selectEntry == NULL)
		printf("没有这个文件 !\n");
	else{
		printf("文件[%s]已打开,输入close关闭.\n", file);
		int c = 0;
		while(1){
			if(c == 1)
				break;
			switch(getCode()){
				case 5:
					read();
					break;
				case 6:
					write();
					break;
				case 7:
					close();
					c = 1;
					break;
				default:
					printf("无效的指令\n");
			}
		}
	}
}

/**
 * 打开文件
 */
void read(){
	FILE* bfp = fopen(blockspath, "r");
	if(bfp == NULL){
		printf("不存在磁盘文件 !\n");
		while(1);
	}
	else{             //打开磁盘文件
		int i;
		char tmp = ' ';
		printf("文件[%s]中的内容如下:\n", selectEntry->ind.filename);
		if(selectEntry->ind.size == 0)
			printf("内容为空。\n");
		else{
			for(i = 0; i < selectEntry->ind.node_num; i++){
				fseek(bfp,(selectEntry->ind.fat[i].num*(MAX_BLOCK_SIZE-1)), SEEK_SET);
				int j;
				for(j = selectEntry->ind.fat[i].begin; j < selectEntry->ind.fat[i].end; j++){  //-----------------------------------
					tmp = fgetc(bfp);
					printf("%c", tmp);
				}
			}
			printf("\n");
		}
		fclose(bfp);
	}
}

/**
 * 关闭文件
 */
void close(){
	selectEntry = NULL;
	printf("文件已关闭 !\n");
}

/**
 * 写文件
 */
void write(){
	char sel[10];
	char cm_cover[10] = "cover";
	char cm_append[10] = "append";
	while(1){
		printf("请输入指令:\n[append]:在原文件基础上新增数据.\n[cover]:覆盖原数据\n[cancle]:取消\n");
		printf("Command: ");
		scanf("%s", sel);
		if(!strcmpi(sel, cm_cancle)){
			printf("取消 !\n");
			break;
		}
		if(!strcmpi(sel,cm_cover)){
			coverHelp();
			break;
		}
		if(!strcmpi(sel,cm_append))
		{
			if(selectEntry->ind.size==0)
			{
				coverHelp();
			}
			else
			{
				appendHelp();
			}
			break;
		}
		printf("指令无效\n");
	}
}


void initFBT(){
	FILE* fp;
	fp = fopen(fbtpath, "r");
	if(fp == NULL){
		printf("Can not open FBT files.\n");
		while(1);
	}
	else{
		int i = 0;
		while(!feof(fp)){
			fscanf(fp, "%d", &FBT[i++]);
			if(i == MAX_BLOCK_SIZE-1){
				break;
			}
		}
	}
}


/**
 * 获取命令码
 * @return 命令码
 */
int getCode(){
	char cmd[10];
	printf("Command: ");
	scanf("%s",cmd);
	if(!strcmpi(cmd, cm_exit))
	{
		return -1;
	}
	if(!strcmpi(cmd, cm_help))
	{
		return 0;
	}
	if(!strcmpi(cmd, cm_dir))
	{
		return 1;
	}
	if(!strcmpi(cmd, cm_create))
	{
		return 2;
	}
	if(!strcmpi(cmd, cm_delete))
	{
		return 3;
	}
	if(!strcmpi(cmd, cm_open))
	{
		return 4;
	}
	if(!strcmpi(cmd, cm_read))
	{
		return 5;
	}
	if(!strcmpi(cmd, cm_write))
	{
		return 6;
	}
	if(!strcmpi(cmd, cm_close))
	{
		return 7;
	}
	return 8;
}



/**
 * 获取行为
 * @return 若结束，返回-1，否则返回0
 */
int getAction(){
	switch(getCode()){
		case -1:
            return -1;
            break;
		case 0:
			help();
			break;
		case 1:
			dir();
			break;
		case 2:
			create();
			break;
		case 3:
			del();
			break;
		case 4:
			open();
			break;
		default:
			printf("无效的指令\n");
	}
	return 0;
}

/**
 * 创建文件
 * @param dirpath 要创建文件的路径
 * @return 返回创建的路径
 */
FILE* createDir(char *dirpath)
{
	FILE *fp = fopen(dirpath, "w");
	if(fp == NULL){
		printf("Can not create Directory !\n");
		while(1);
	}
	else{
		fclose(fp);
		FILE *f = fopen(dirpath, "r");
		return f;
	}
}

/**
 * 获取当前用户目录文件路径
 * @param username 用户登录账号
 * @return res 当前用户目录文件路径
 */
char *getDirpath(char* username){
	char *res;
	res = (char*)malloc(50*sizeof(char));
	char t[10] = "userdata/";
	char tail[5] = ".dir";
	strcpy(res, t);
	strcat(res, username);
	strcat(res, tail);
	return res;
}

/**
 * 初始化Dir
 */
void initDir(char *dirpath){
	FILE *p ;
	p = fopen(dirpath, "r");
	if(NULL == p){
		p = createDir(dirpath);
	}
	dirEntry* pp = (dirEntry*)malloc(sizeof(dirEntry));
	pp->next = NULL;
	DIR = NULL;
	int nm = 0;
	while(!feof(p)){
		if(nm != 0){
			dirEntry* pt = (dirEntry*)malloc(sizeof(dirEntry));
			pt->next = NULL;
			pp->next = pt;
			pp = pt;
		}
		int r = fscanf(p, "%s", pp->ind.filename);
		if(r == EOF)
			break;
		fscanf(p, "%d", &pp->ind.num);
		if(max_node < pp->ind.num)
			max_node = pp->ind.num;
		fscanf(p, "%s", pp->ind.code);
		fscanf(p, "%d", &pp->ind.size);
		if(pp->ind.size != 0){
			fscanf(p, "%d" , &pp->ind.node_num);
			for(int j = 0;j < pp->ind.node_num; j++){
				fscanf(p, "%d", &pp->ind.fat[j].num);
				fscanf(p, "%d", &pp->ind.fat[j].begin);
				fscanf(p, "%d", &pp->ind.fat[j].end);
			}
		}

		curr_inode_num++;
		if(curr_inode_num == MAX_INODE_NUM){
			break;
		}
		if(nm == 0){
			DIR = pp;
			nm = 1;
		}
		currEntry = pp;
	}
	fclose(p);
}



/**
 * 初始化Dir和FBT
 */
void init(){
	initDir(getDirpath(currUser.account));
	initFBT();
}

/**
 * 启动函数
 */
void start(){
	printf("\n         文件管理系统 v1.0\n");
	printf("\n    支持在任意目录下运行该程序\n");
	FILE *ufp = fopen(userspath, "r");         //检查是否第一次使用本系统，查看是否存在users文件
	if(NULL == ufp){
		printf("\n       这是您首次使用该系统\n\n");
		createDataDir();
		FILE *fp = fopen(userspath, "w");
		createBlocksDisk();                          //第一次使用，需要开辟 [磁盘] 空间
		if(NULL == fp){
			printf("\nERROR!\nCtrl+C to quit\n");
			while(1);
		}
		else
			fclose(fp);
	}
	else
		fclose(ufp);
	while(LgRg());
	printf("已进入FMS文件管理虚拟系统\n");
	dirpath = getDirpath(currUser.account);
	init();
	printf("系统初始化已完成,输入help查看命令帮助,输入exit/quit退出系统\n");
	while(getAction()!=-1);

}



/**
 * 获取当前登录用户信息
 */
void getUser(){
	while(1){
		printf("account(length < 10): ");
		scanf("%s", currUser.account);
		if(strlen(currUser.account) <= 10)
			break;
		else
			printf("账户名最长为10位.\n");
	}
	while(1){
		printf("password(length<10): ");
		scanf("%s", currUser.password);
		if(strlen(currUser.password)<=10)
			break;
		else
			printf("账户密码最长为10位.\n");
	}
}

/**
 * 登录
 * @return 若登录成功返回0，否则返回-1
 */
int login(){
	getUser();
	FILE *ffp = fopen(userspath, "r");
	if(NULL == ffp){
		printf("FILE ERROR !\n");
		exit(0);
	}
	else{
		user temp;
		while(!feof(ffp)){
			fscanf(ffp, "%s", temp.account);
			fscanf(ffp, "%s", temp.password);
			if((!strcmpi(temp.account, currUser.account)) && (!strcmpi(temp.password, currUser.password))){
				fclose(ffp);
				return 0;
			}
		}
		fclose(ffp);
		return -1;
	}
}

/**
 * 注册
 * @return 注册成功返回0，并将注册的用户信息写入用户文件，否则返回-1
 */
int regist(){
	getUser();
	FILE *fp = fopen(userspath, "a");
	if(NULL == fp)	{
		printf("FILE ERROR !\n");
		return -1;
	}
	else{
		fprintf(fp, "%s ", currUser.account);
		fprintf(fp, "%s\n", currUser.password);
		fclose(fp);
		return 0;
	}
}



/**
 * 登录或注册
 * @return 登录或失败返回-1，登录或注册成功返回0
 */
int LgRg(){
	char type = lgOrRg();
	if('l' == type){
		if(login()){
			printf("登录失败 ！\n");
			return -1;
		}
		else{
			printf("登录成功 ！\n");
			return 0;
		}
	}
	else{
		if(regist()){
			printf("注册失败 ！\n");
			return -1;
		}
		else{
			printf("注册成功 ！\n");
			return 0;
		}
	}
}

/**
 * 输出目录中所有文件信息
 */
void dir(){
	printf("文件名\t物理地址\t保护码\t\t文件长度\n");
	int i=0;
	dirEntry *pt = DIR;
	while(pt!=NULL)	{
		printf("%s\t%d\t\t%s\t\t%d\n", pt->ind.filename, i++, pt->ind.code, pt->ind.size);
		pt=pt->next;
	}
}

/**
 * 保存文件
 */
void saveDir(){
	//printf("dirpath = %s\n",dirpath);
	FILE *fp = fopen(getDirpath(currUser.account), "w");
	if(fp == NULL){
		printf("updateDirDisk ERROR !\n");
		while(1);
	}
	else{
		int j;
		dirEntry *p = DIR;
		while(p != NULL)
		{
			fprintf(fp, " %s", p->ind.filename);
			fprintf(fp, " %d", p->ind.num);
			fprintf(fp, " %s", p->ind.code);
			fprintf(fp, " %d", p->ind.size);
			if(p->ind.size != 0){
				fprintf(fp, " %d", p->ind.node_num);
				for(j = 0; j < p->ind.node_num; j++){
					fprintf(fp, " %d", p->ind.fat[j].num);
					fprintf(fp, " %d", p->ind.fat[j].begin);
					fprintf(fp, " %d", p->ind.fat[j].end);
				}
			}
			p = p->next;
		}
		fclose(fp);
	}
}

/**
 * 存数据块
 */
void saveBlock(block bk)
{
	FILE *fp = fopen(blockspath, "r+");
	if(fp == NULL){
		printf("SaveBlock ERROR !\n");
	}
	else{
		long offset = bk.num * (MAX_BLOCK_SIZE-1);
		fseek(fp, offset, SEEK_SET);
		fwrite(bk.content, sizeof(char), strlen(bk.content), fp);
		fclose(fp);
		FBT[bk.num] = 1;
	}
}

/**
 * 保存数据块状态信息
 */
void saveFBT(){
	FILE* fp = fopen(fbtpath, "w");
	if(fp == NULL){
		printf("SaveFBT ERROR !\n");
		while(1);
	}
	else{
		int i;
		for(i = 0; i < MAX_BLOCKS_NUM; i++){
			fprintf(fp, " %d", FBT[i]);
		}
		fclose(fp);
	}
}

//数据块选择算法
int getFreeBlock(bool isAdd)    //因为文件最多占用两个数据块，因此在未用数据块的时候要保证至少有两个数据块
{                               //若已经用了一个数据块，则只要保证再有一个数据块就够了
	int pos = -1;
	int num = 0;
	int k;
	for(k = 0; k < MAX_BLOCKS_NUM; k++)	{
		if(FBT[k] == 0){
			if(isAdd == true)
				return k;
			if(num == 0){
				pos = k;
				num++;
			}
			else
				return pos;
		}
	}
	return -1;
}
