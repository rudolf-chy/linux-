#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>
#include <unistd.h>
#define _USER_   "root"
#define _HOST_  "127.0.0.1"
#define _PASSWD_    "qq976910756"
#define _DB_        "seckill"
#define _PORT_      3306

void show_information(MYSQL_RES *res, MYSQL *mysql){
    int i;
    MYSQL_ROW row;
    unsigned int num_fields;
    MYSQL_FIELD *field;
    num_fields = mysql_num_fields(res);
    printf("num_fields = [%d]\n", num_fields);
    for(i = 0; i < num_fields, field = mysql_fetch_field(res); i ++){
        printf(" %s\t", field->name);
    }
    printf("\n");
    printf("-----------------------------------\n");
    while((row = mysql_fetch_row(res))){
        for(i = 0; i < num_fields; i ++){
            printf(" %s \t", row[i] ? row[i] : NULL);
        }
        printf("\n");
    }

    mysql_free_result(res);
    printf("-----------------------------------\n");
    printf("    %ld rows in set\n", (long)mysql_affected_rows(mysql));
}

int main(int argc, char *argv[]){
    int ret, i;
    MYSQL *mysql;
    MYSQL_RES *res;
    char strsql[512] = {0};
    mysql = mysql_init(NULL);
    if(mysql == NULL){
        printf("mysql_init error\n");
        return -1;
    }
    mysql = mysql_real_connect(mysql,
                            _HOST_,
                            _USER_,
                            _PASSWD_,
                            _DB_,
                            0,
                            NULL,
                            0);
    if(mysql == NULL){
        printf("mysql_real_connect error\n");
        return -1;
    }
    printf("connect mysql ok\n");
    if(mysql_set_character_set(mysql, "utf8") != 0){
        printf(" mysql_set_character_set error\n");
        mysql_close(mysql);
        exit(1);
    }
    while(1){
        memset(strsql, 0, sizeof(strsql));
        write(STDOUT_FILENO, "yoursql>", 8);
        read(STDIN_FILENO, strsql, sizeof(strsql));
        if(strncasecmp(strsql, "quit", 4) == 0){
            printf("client 退出处理\n");
            break;
        }
        ret = mysql_query(mysql, strsql);
        if(ret){
            printf("mysql_query error\n");
            continue;
        }
        res = mysql_store_result(mysql);
        if(res != NULL){
            show_information(res, mysql);
        }else{
            printf(" Query OK, %ld row affected\n", (long)mysql_affected_rows(mysql));
        }
    }

    mysql_close(mysql);

    return 0;
}