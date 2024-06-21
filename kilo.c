#include "kilo.h"

/**
 * 自定义终端编辑器
 */
int main(int argc, char const *argv[]) {
    // 初始化编辑器
    if (initEditor() == -1) {
        perror("init error.");
        return 0;
    }

    // 获取文件名
    if (argc >= 2) {
        int len = strlen(argv[1]);
        F.filename = malloc(len + 1);
        memcpy(F.filename, argv[1], len);
        F.filename[len] = '\0';
    }

    // 设置终端为原始模式
    if (enableRawMode() == -1) {
        perror("set raw mode occurred error.");
        return 0;
    }

    // 读取文件到内存中
    if (F.filename) {
        if (readFile(F.filename, &F) == -1) {
            perror("read file.");
            return 0;
        }
        if (F.fileStatus != NULL) {
            char *msg = malloc(strlen(F.filename) + strlen(F.fileStatus) + 2);
            int idx = 0;
            memcpy(&msg[idx], F.filename, strlen(F.filename));
            idx += strlen(F.filename);
            msg[idx] = ' ';
            idx += 1;
            memcpy(&msg[idx], F.fileStatus, strlen(F.fileStatus));
            idx += strlen(F.fileStatus);
            msg[idx] = '\0';
            E.statusMsg = msg;
        } else {
            E.statusMsg = F.filename;
        }
    }

    // 刷新出一整个屏幕的空间，然后定位光标到屏幕左上角
    write(STDOUT_FILENO, screen, screensize);
    renderTextArea();

    while (E.exitFlag == 0) {
        if (E.refreshFlag) {
            renderTextArea();
        }
        renderStatusLine(E.statusMsg);
        refreshCursor(E.ry - E.rowoff + 1, E.rx - E.coloff + 1);
        processKeypress();
    }
    refreshCursor(1, 1);
    write(STDOUT_FILENO, CLEAR_SCREEN, strlen(CLEAR_SCREEN));

    free(F.filename);
    return 0;
}

int readKey() {
    int bufLen = 16;
    char buf[bufLen];
    memset(buf, 0, bufLen);
    read(STDIN_FILENO, &buf, 1);

    if (buf[0] == ESC) {
        // 每次循环都要清空集合，否则不能检测描述符变化
        FD_ZERO(&readfds);
        // 添加标准输入到读事件的文件描述符中
        FD_SET(STDIN_FILENO, &readfds);

        // select函数会不断修改timeout的值，所以每次循环都应该重新赋值
        timeout.tv_sec = 0;
        timeout.tv_usec = 1;  // 设置超时时间是1毫秒

        int ret = select(1, &readfds, NULL, NULL, &timeout);
        if (ret > 0) {
            // 判断标准输入是否可读
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                int len = read(STDIN_FILENO, &buf[1], bufLen - 1);
                if (len <= 0) {
                    return buf[0];
                }
                for (int i = 0; i < CmdKeyMapListLen; i++) {
                    if (strncmp(CmdKeyMapList[i].escapeSeq, buf, len + 1) == 0) {
                        return CmdKeyMapList[i].val;
                    }
                }
            }
        }
    }
    return buf[0];
}

int getRowNum(Erow *erow) {
    if (erow->sizeTab < E.screencols) {
        return 1;
    }
    if (erow->sizeTab % E.screencols != 0) {
        return erow->sizeTab / E.screencols + 1;
    } else {
        return erow->sizeTab / E.screencols;
    }
}

int convertCyToRy(int cy) {
    if (cy < E.rowoff) {
        return cy;
    }
    int ry = E.rowoff;

    for (int i = E.rowoff; i < cy; i++) {
        ry += getRowNum(&F.pErow[i]);
    }
    return ry;
}

int convertCxToRx(Erow *erow, int cx) {
    int rx = 0;

    for (int i = 0; i < cx; i++) {
        if (erow->row[i] == TAB) {
            rx += (TAB_STOP) - (rx % TAB_STOP);
        } else {
            rx++;
        }
    }
    return rx;
}

int convertRxToCx(Erow *erow, int rx) {
    int cx = 0;

    int rxr = 0;
    for (int i = 0; i < erow->size; i++) {
        if (erow->row[i] == TAB) {
            rxr += (TAB_STOP) - (rxr % TAB_STOP);
        } else {
            rxr++;
        }
        cx++;
        if (rxr >= rx) {
            break;
        }
    }
    return cx;
}

int getRowLimit(Erow *erow) {
    int rowLimit = 0;
    if (E.cmdMode == 0) {
        rowLimit = erow->size > 0 ? erow->size - 1 : 0;
    } else {
        rowLimit = erow->size;
    }
    return rowLimit;
}

void moveCursor(int key) {
    int rowoff = E.rowoff;
    int coloff = E.coloff;
    Erow *erow = &F.pErow[E.cy];
    int rowLimit = getRowLimit(erow);

    switch (key) {
        case ARROW_UP:
            if (E.cy > 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if (E.cy < F.size - 1) {
                E.cy++;
            }
            break;
        case ARROW_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                // 光标到达第一列后移到上一行末尾
                E.cy--;
                E.cx = getRowLimit(&F.pErow[E.cy]);
            }
            break;
        case ARROW_RIGHT:
            if (E.cx < rowLimit) {
                E.cx++;
            } else if (E.cy < F.size - 1) {
                // 光标到达最后一列后移动到下一行
                E.cy++;
                E.coloff = 0;
                E.cx = 0;
            }
            break;
    }

    // 光标换行后调整光标的横坐标
    erow = &F.pErow[E.cy];
    rowLimit = getRowLimit(erow);
    if (key == ARROW_UP || key == ARROW_DOWN) {
        if (E.coloff != 0 && erow->sizeTab < E.screencols) {
            E.coloff = 0;
        }
        if (E.cx > rowLimit) {
            E.cx = rowLimit;
        }
    }

    if (E.softBRFlag == 0) {
        editorScroll();
    } else {
        editorScrollWithSBR();
    }

    // 是否全局刷新
    if (rowoff != E.rowoff || coloff != E.coloff) {
        E.refreshFlag = 1;
    } else {
        E.refreshFlag = 0;
    }
    // 在查看模式下，只要移动光标，就清除状态行左边的数据，通常它是文件名
    if (E.cmdMode == 0) {
        E.statusMsg = "";
    }
}

void editorScrollWithSBR() {
    int ry = convertCyToRy(E.cy);
    // 当光标越过屏幕底部的时候
    if (ry >= (E.textAreaRows + E.rowoff)) {
        E.rowoff = ry - E.textAreaRows + 1;
    }
    if (ry < E.rowoff && E.rowoff > 0) {
        E.rowoff = ry;
    }
    E.ry = convertCyToRy(E.cy);

    // 根据cx来更新rx
    E.rx = convertCxToRx(&F.pErow[E.cy], E.cx);
    if (E.rx > E.screencols - 1) {
        E.ry += E.rx / E.screencols;
        E.rx = E.rx % E.screencols;
        E.refreshFlag = 1;
        
        if (E.ry >= E.textAreaRows) {
            E.rowoff = E.ry - E.textAreaRows + 1;
        }
    }
}

void editorScroll() {
    // 根据cx来更新rx
    E.rx = convertCxToRx(&F.pErow[E.cy], E.cx);
    E.ry = E.cy;

    // 调整行偏移量和列偏移量
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.textAreaRows + E.rowoff) {
        E.rowoff = E.cy - E.textAreaRows + 1;
    }
    if (E.rx < E.coloff) {
        E.coloff = E.rx;
    }
    if (E.rx >= E.screencols + E.coloff) {
        E.coloff = E.rx - E.screencols + 1;
    }
}

int processKeypress() {
    int key = readKey();

    if (key >= 1000 && key <= 1003) {
        moveCursor(key);
        return key;
    }

    if (E.cmdMode == 1) {
        renderStatusLine(F.filename);
        charInsert(key);
        return key;
    }

    if (E.cmdMode == 0) {
        switch (key) {
            // 退出程序
            case CTRL_C:
                editorSave(F.filename);
                E.exitFlag = 1;
                break;

            case CTRL_Q:
                E.exitFlag = 1;
                break;
            
            case CTRL_S:
                editorSave(F.filename);
                break;

            case 'i':
                E.statusMsg = "--INSERT--";
                E.cmdMode = 1;
                break;
        }
    }

    return key;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    // 获取屏幕大小
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    }
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
}

int initEditor() {
    // 光标位置
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.ry = 0;
    E.rowoff = 0;
    E.coloff = 0;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) {
        return -1;
    }
    E.screencols--; // 屏幕的最后一列不显示数据

    // 最后一行空出来作为状态行
    E.statusRow = E.screenrows;
    E.textAreaRows = E.screenrows - 1;

    // 屏幕数据
    screensize = E.screenrows * E.screencols + 1;
    screen = malloc(screensize);
    memset(screen, ' ', screensize);
    screen[screensize - 1] = '\0';

    // 编辑模式
    E.cmdMode = 0;
    E.statusMsg = F.filename;
    E.refreshFlag = 0;
    E.exitFlag = 0;

    // 读取配置文件
    E.confFileName = "ki.conf";
    EContent CONF = ECONTENT_INIT;
    if (readFile(E.confFileName, &CONF) == -1) {
        // 默认配置
        E.softBRFlag = 0;
    }
    if (parseConfFile(&CONF) == -1) {
        // 默认配置
        E.softBRFlag = 0;
    }
    return 0;
}

int parseConfFile(EContent *CONF) {
    // 解析配置文件
    for (int i = 0; i < CONF->size; i++) {
        char *row = CONF->pErow[i].row;
        if (strlen(row) == 0 || row[0] == '#') {
            continue;
        }
        for (int j = 0; j < confKeyArrLen; j++) {
            if (strncmp(row, confKeyArr[j], strlen(confKeyArr[j])) == 0) {
                // 读取配置：是否开启软换行
                if (j == 0) {
                    char *value = strchr(row, '=');
                    int flag = 0;
                    int items_scanned = sscanf(value, "=%d", &flag);
                    if (items_scanned != 1) {
                        return -1;
                    }
                    E.softBRFlag = flag;
                }
            }
        }
    }
    return 0;
}

/* 文件 */

int editorSave(char *filename) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        return -1;
    }

    Str str = STR_INIT;
    for (int i = 0; i < F.size; i++) {
        strAppend(&str, F.pErow[i].row, F.pErow[i].size);
        strAppend(&str, "\n", 1);
    }

    fwrite(str.chars, str.size, 1, file);
    fclose(file);
    free(str.chars);
    return 0;
}

// 读取文件内容
int readFile(char *filename, EContent *F) {
    F->filename = filename;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        F->fileStatus = "[New File]";
        insertRow(F, 0, "", 0);
        F->sumChars = 0;
        return 0;
    } 

    // 获取文件大小：先将读写指针移动到文件末尾，然后获取当前指针的位置，这个位置就代表文件大小，
    // 然后再调用rewind函数将文件指针移到开头
    fseek(file, 0, SEEK_END);
    int fileSize = ftell(file);
    rewind(file);

    // 读取文件内容到缓冲区
    char * buffer = malloc(fileSize + 1);
    fread(buffer, sizeof(char), fileSize, file);
    buffer[fileSize] = '\0';
    close(file->_fileno);

    int lineStart = 0;
    int lineEnd = 0;
    int lineIdx = 0;
    while (lineEnd <= fileSize) {
        if (buffer[lineEnd] == LINE_BREAK) {
            insertRow(F, lineIdx, &buffer[lineStart], lineEnd - lineStart);  // 不复制换行符
            lineIdx++;
            lineStart = lineEnd + 1;
        } else if (lineEnd == fileSize) {
            // 如果文件结尾没有换行符
            if (lineEnd - lineStart > 0) {
                insertRow(F, lineIdx, &buffer[lineStart], lineEnd - lineStart + 1);
                lineIdx++;
                lineStart = lineEnd + 1;
            }
        }
        lineEnd++;
    }

    if (fileSize == 0) {
        insertRow(F, lineIdx, "", 0);
    }

    int sumChars = 0;
    for (int i = 0; i < F->size; i++) {
        sumChars += F->pErow[i].size + 1;  // 每行结尾的换行符
    }
    F->sumChars = sumChars;
    return 0;
}

void charInsert(int c) {
    if (c == ESC) {
        E.cmdMode = 0;
        E.statusMsg = F.filename;
        return;
    }

    int rowoff = E.rowoff;
    int coloff = E.coloff;

    if (c == DEL_KEY) {
        moveCursor(ARROW_RIGHT);
        deleteCurChar();
    } else if (c == BACKSPACE) {
        deleteCurChar();
    } else if (c == ENTER) {
        insertEnterCharInCurPos();
    } else if (c == TAB || !(c < 32 || c > 128)) {
        stupidInsert(c);
    }

    if (E.softBRFlag == 0) {
        editorScroll();
    } else {
        editorScrollWithSBR();
    }

    if (rowoff != E.rowoff || coloff != E.coloff) {
        E.refreshFlag = 1;
    }

    if (E.refreshFlag != 1) {
        renderRow();
    }
}

int appendRowToScreenSBR(Erow *erow, Str *str) {
    char *s;
    int len;
    if (erow->sizeTab > E.screencols) {
        int i = 0;
        while (1) {
            s = &erow->rowTab[i];
            if (erow->sizeTab - i > E.screencols) {
                len = E.screencols;
            } else {
                len = erow->sizeTab - i;
            }
            strAppend(str, CLEAR_LINE_REST, strlen(CLEAR_LINE_REST));
            strAppend(str, s, len);
            strAppend(str, "\r\n", 2);
            i += len;
            if (i >= erow->sizeTab) {
                break;
            }
        }

        int rowNum;
        if (erow->sizeTab % E.screencols != 0) {
            rowNum = erow->sizeTab / E.screencols + 1;
        } else {
            rowNum = erow->sizeTab / E.screencols;
        }
        return rowNum;
    } else {
        s = &erow->rowTab[0];
        len = erow->sizeTab;
        strAppend(str, CLEAR_LINE_REST, strlen(CLEAR_LINE_REST));
        strAppend(str, s, len);
        strAppend(str, "\r\n", 2);
        return 1; // 添加了1行数据
    }
}

// 全部刷新屏幕时使用，刷新一整行文本
void appendRowToScreen(Erow *erow, Str *str) {
    char *s;
    int len;
    if (E.coloff < erow->sizeTab) {
        s = &erow->rowTab[E.coloff];
        len = erow->sizeTab - E.coloff;
        if (len > E.screencols) {
            len = E.screencols;
        }
    } else {
        s = "";
        len = 1;
    }

    strAppend(str, CLEAR_LINE_REST, strlen(CLEAR_LINE_REST));
    strAppend(str, s, len);
    strAppend(str, "\r\n", 2);
}

// 刷新当前行，用户编辑文本时使用，部分刷新屏幕
void renderRow() {
    int rx = convertCxToRx(&F.pErow[E.cy], E.cx - 1);
    refreshCursor(E.ry - E.rowoff + 1, rx - E.coloff + 1);

    Erow *erow = &F.pErow[E.cy];
    char *s = &erow->rowTab[rx];
    int len = erow->sizeTab - rx;
    if (len > (E.screencols - rx)) {
        len = E.screencols - rx;
    }

    write(STDOUT_FILENO, s, len);
    write(STDOUT_FILENO, CLEAR_LINE_REST, strlen(CLEAR_LINE_REST));
    refreshCursor(E.ry - E.rowoff + 1, E.rx - E.coloff + 1);
}

void insertEnterCharInCurPos() {
    Erow *erow = &F.pErow[E.cy];

    if (E.cx < erow->size) {
        insertRow(&F, E.cy + 1, &erow->row[E.cx], erow->size - E.cx);
    } else if (E.cx == erow->size) {
        insertRow(&F, E.cy + 1, "", 0);
    }

    erow = &F.pErow[E.cy];
    erow->size = E.cx;
    erow->row[E.cx] = '\0';
    updateRowTabByRow(erow);

    E.cx = 0;
    E.coloff = 0;
    E.cy++;
    F.sumChars++;
    E.refreshFlag = 1;
}

void deleteCurChar() {
    Erow *erow = &F.pErow[E.cy];
    if (E.cx > 0) {
        memmove(&erow->row[E.cx - 1], &erow->row[E.cx], erow->size - E.cx);
        erow->size--;
        erow->row[erow->size] = '\0';
        updateRowTabByRow(erow);

        E.cx--;
        F.sumChars--;
    } else if (E.cy > 0) {
        Erow *preErow = &F.pErow[E.cy - 1];

        int cx = preErow->size;
        // 将下一行的内容复制到上一行
        preErow->row = realloc(preErow->row, preErow->size + erow->size + 1);
        memcpy(&preErow->row[preErow->size], erow->row, erow->size);
        preErow->size += erow->size;
        preErow->row[preErow->size] = '\0';
        updateRowTabByRow(preErow);

        deleteRow(&F, E.cy);

        E.cy--;
        E.cx = cx;
        F.sumChars--;
        E.refreshFlag = 1;
    }
}

void stupidInsert(int c) {
    Erow *erow = &F.pErow[E.cy];
    erow->row = realloc(erow->row, erow->size + 1);
    if (erow->size - E.cx > 0) {
        memmove(&erow->row[E.cx + 1], &erow->row[E.cx], erow->size - E.cx);
    }
    erow->row[E.cx] = c;
    erow->size++;
    updateRowTabByRow(erow);

    E.cx++;
    F.sumChars++;
}

void deleteRow(EContent *F, int rowIdx) {
    Erow *erow = &F->pErow[rowIdx];
    free(erow->row);
    free(erow->rowTab);

    memmove(F->pErow + rowIdx, F->pErow + rowIdx + 1, 
       (F->size - rowIdx) * sizeof(Erow));
    F->size--;
}

void insertRow(EContent *F, int rowIdx, char *buffer, int len) {
    F->pErow = realloc(F->pErow, (F->size + 1) * sizeof(Erow));
    // 如果要插入的行不是最后一行，那么rowIdx之后的所有行都要后移一位
    if (rowIdx != F->size) {
        memmove(F->pErow + rowIdx + 1, F->pErow + rowIdx, (F->size - rowIdx) * sizeof(Erow));
    }
    F->size++;

    Erow *pErow = &F->pErow[rowIdx];
    pErow->row = malloc(len + 1);
    strncpy(pErow->row, buffer, len);
    pErow->row[len] = '\0';
    pErow->size = len;

    // 将tab键转换为空格
    updateRowTabByRow(pErow);
}

void updateRowTabByRow(Erow *erow) {
    // 将tab键转换为空格
    int rowTabLen = 0;
    for (int i = 0; i < erow->size; i++) {
        if (erow->row[i] == '\t') {
            rowTabLen += TAB_STOP - (rowTabLen % TAB_STOP);
        } else {
            rowTabLen++;
        }
    }

    erow->rowTab = malloc(rowTabLen + 1);
    erow->sizeTab = rowTabLen;
    
    int j = 0;
    for (int i = 0; i < erow->size; i++) {
        if (erow->row[i] == '\t') {
            int len = TAB_STOP - (j % TAB_STOP);
            while (len--) {
                erow->rowTab[j] = ' ';
                j++;
            }
        } else {
            erow->rowTab[j] = erow->row[i];
            j++;
        }
    }
    erow->rowTab[rowTabLen] = '\0';
}

void renderTextArea() {
    refreshCursor(1, 1);

    Str str = STR_INIT;
    int rowNum;
    int filerow = E.rowoff;
    for (int i = 0; i < E.textAreaRows; i += rowNum) {
        if (F.size > 0 && filerow < F.size) {
            Erow *erow = &F.pErow[filerow];

            if (E.softBRFlag == 0) {
                // 不使用软换行
                rowNum = 1;
                appendRowToScreen(erow, &str);
            } else {
                // 使用软换行
                rowNum = appendRowToScreenSBR(erow, &str);
            }
        } else {
            rowNum = 1;
            strAppend(&str, CLEAR_LINE_REST, strlen(CLEAR_LINE_REST));
            if (i == 0) {
                strAppend(&str, "\r\n", 2);
            } else {
                strAppend(&str, "~\r\n", 3);
            }
        }
        filerow++;
    }

    // 针对软换行进行优化，如果文件内容超过一个屏幕大小，截取一个屏幕大小的数据
    if (E.softBRFlag == 1) {
        int count = 0;
        int idx = 0;
        for (int i = 0; i < E.textAreaRows; i++) {
            char *lineEnd = strchr(&str.chars[idx], '\n');
            if (lineEnd != NULL) {
                idx = lineEnd - str.chars + 1;
                count++;
            }
            if (count >= E.textAreaRows) {
                str.chars[idx] = '\0';
                str.size = idx;
                break;
            }
        }
    }

    write(STDOUT_FILENO, str.chars, str.size);
}

void renderStatusLine(char *msg) {
    int bufLen = E.screencols;
    char buf[bufLen];

    int msgLen = strlen(msg);

    int rightBufLen;
    char rightBuf[32];
    memset(rightBuf, 0, 32);

    rightBufLen = snprintf(rightBuf, 32, "%dL,%dC  %d,%d", 
            F.size, F.sumChars, E.cy + 1, E.cx + 1);

    int middleLen = bufLen - msgLen - rightBufLen;

    memcpy(&buf[0], msg, msgLen);
    memset(&buf[msgLen], ' ', middleLen);
    memcpy(&buf[msgLen + middleLen], rightBuf, rightBufLen);

    refreshCursor(E.statusRow, 1);
    write(STDOUT_FILENO, buf, bufLen);
}

void refreshCursor(int row, int col) {
    int bufLen = 16;
    char buf[bufLen];

    int len = snprintf(buf, bufLen, LOCATE_CURSOR, row, col);
    write(STDOUT_FILENO, buf, len);
}

void strAppend(Str *str, char *s, int len) {
    if (str->chars == NULL) {
        str->chars = malloc(len);
    } else {
        str->chars = realloc(str->chars, str->size + len);
    }
    memcpy(&str->chars[str->size], s, len);
    str->size += len;
}


void strFree(Str *str) {
    free(str->chars);
}

int enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &origTermios) == -1) {
        return -1;
    }
    atexit(disableRawMode);

    rawTermios = origTermios;
    // IXON：用于设置或检查终端是否启用了软件流控制，
    //   Ctrl+S 和 Ctrl+Q：按下Ctrl+S将停止数据传输，按下Ctrl+Q将恢复数据传输
    // ICRNL：设置或检查终端是否将输入的回车符（\r）转换为换行符（\n）
    rawTermios.c_iflag &= ~(ICRNL | IXON);
    // rawTermios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    // OPOST：用于设置或检查终端是否启用了输出处理，换行符（\n）转换为回车换行符（\r\n）
    rawTermios.c_oflag &= ~(OPOST);

    // rawTermios.c_cflag |= (CS8);
    
    // ECHO：回显，表示终端会显示用户的输入。这里关闭了回显，
    // ICANON：经典模式。在这里关闭了行缓冲
    // ISIG：信号模式。在这里关闭了信号模式，程序不再处理Ctrl+C和Ctrl+Z
    // IEXTEN：用于设置或检查终端是否启用了扩展输入处理。程序不再处理ctrl+v
    rawTermios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    // 设置read函数最多等待100毫秒，没有数据也返回

    // VMIN：设置read函数返回前必须接收到的最小字节数
    // rawTermios.c_cc[VMIN] = 0;
    // VTIME：设置了read()函数等待输入的最长时间，以十分之一秒为单位
    // rawTermios.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTermios) == -1) {
        return -1;
    }
    return 0;
}

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
}

