# MailFront - Qt/C++ 邮件客户端

基于 **Qt 6 + VMime** 构建的跨平台邮件客户端，支持 SMTP 发信和 IMAP 收信。

## 项目概览

```
mail_front/
├── CMakeLists.txt                 # CMake 构建 (C++17, Qt6/Qt5)
├── main.cpp                       # 入口：QApplication → mailApp.show()
├── app/                           # 窗口层
│   ├── mailapp.h/.cpp/.ui         # 主窗口 (QMainWindow)
│   ├── composewindow.h/.cpp       # 写邮件窗口 (QMainWindow)
│   └── settingsdialog.h/.cpp      # 账户设置对话框 (QDialog)
├── models/                        # Model/View 层
│   └── maillistmodel.h/.cpp       # 邮件列表模型 (QAbstractListModel)
├── network/                       # 网络层 (VMime 封装)
│   ├── smtpclient.h/.cpp          # SMTP 发信 (QtConcurrent 异步)
│   ├── imapclient.h/.cpp          # IMAP 收信 (QtConcurrent 异步)
│   └── VSMimeNetWorkAdapter.*     # [已废弃] 早期原型
├── services/                      # 业务服务层
│   ├── accountmanager.h/.cpp      # 账户管理 (QSettings 持久化)
│   ├── mailstore.h/.cpp           # 本地存储 (SQLite via QSqlDatabase)
│   └── cryptohelper.h/.cpp        # AES-256 密码加密 (CommonCrypto)
├── widgets/                       # 自定义控件
│   ├── foldertree.h/.cpp          # 文件夹树 (QTreeView)
│   └── mailpreview.h/.cpp         # 邮件预览面板 (QWidget)
└── core/                          # 数据结构 (纯 struct)
    ├── email.h                    # 邮件数据
    ├── account.h                  # 账户配置
    └── folder.h                   # 文件夹信息
```

## 架构分层

```
┌─────────────────────────────────────────────────┐
│  UI Layer (Qt Widgets)                          │
│  mailApp / ComposeWindow / SettingsDialog        │
├─────────────────────────────────────────────────┤
│  Model Layer (Model/View)                       │
│  MailListModel ← QListView                      │
│  QStandardItemModel ← FolderTree(QTreeView)     │
├─────────────────────────────────────────────────┤
│  Service Layer                                  │
│  AccountManager / MailStore / CryptoHelper      │
├─────────────────────────────────────────────────┤
│  Network Layer (异步非阻塞)                      │
│  SmtpClient / ImapClient — QtConcurrent + VMime │
├─────────────────────────────────────────────────┤
│  Data Layer                                     │
│  SQLite (emails table) / QSettings (accounts)   │
└─────────────────────────────────────────────────┘
```

## Qt 核心技术点

| 技术 | 应用位置 | 说明 |
|------|---------|------|
| **Signal & Slot** | 全局通信 | 窗口间通信、网络层回调 UI |
| **Model/View** | MailListModel + QListView | 数据与显示分离，自定义 Role |
| **QtConcurrent + QFutureWatcher** | SmtpClient / ImapClient | 线程池执行阻塞 I/O，UI 不卡顿 |
| **QSettings** | AccountManager | 跨平台账户配置持久化 |
| **QSqlDatabase** | MailStore | SQLite 本地邮件缓存 |
| **MOC / Q_OBJECT** | 所有 QObject 子类 | 信号槽元对象系统 |
| **QObject 父子树** | 所有组件 | 自动内存管理 |
| **QMainWindow** | mailApp, ComposeWindow | MenuBar + ToolBar + StatusBar + CentralWidget |
| **QDialog** | SettingsDialog | 模态设置窗口 |
| **QSplitter** | 主窗口 3 栏布局 | 可拖拽分隔 |
| **QTextEdit / QTextBrowser** | 写邮件 / 预览 | 富文本 HTML 编辑和渲染 |
| **Cross-thread signal** | 网络线程 → UI 线程 | Qt 自动处理跨线程信号 |

## 构建与运行

```bash
cd mail_front/build
cmake ..
make -j4
./mail_front.app/Contents/MacOS/mail_front
```

依赖：Qt6 (Widgets, Concurrent, Sql)、VMime、macOS CommonCrypto。

## 关键设计决策

### 1. 异步非阻塞网络模式

所有 VMime 网络操作都通过 `QtConcurrent::run()` 在线程池中执行，用 `QFutureWatcher` 将完成信号投递回 UI 线程：

```
UI 线程                        工作线程
  │                              │
  ├─ sendEmail() ───────────────>│ 阻塞 SMTP I/O
  │  (立即返回，UI 保持响应)      │
  │                              ├─ tr->connect()
  │                              ├─ tr->send(msg)
  │                              └─ tr->disconnect()
  │<── signal: emailSent() ─────┤
  │<── signal: errorOccurred() ─┤
```

### 2. SMTP AUTH 认证流程

- VMime 默认不启用 AUTH（`options.need-authentication` 默认为 false），需手动开启
- 客户端发送 `AUTH PLAIN`（一步发送 base64 凭证）
- 测试阶段使用 `DummyCertVerifier` 跳过 TLS 证书验证

### 3. 密码本地存储

使用 AES-256-CBC 加密账户密码后才写入 QSettings，密钥由 `QSysInfo::machineUniqueId()` 经 SHA-256 派生，设备绑定。

### 4. 数据流

- **写邮件**：ComposeWindow → SmtpClient（异步发信） + MailStore（本地保存）
- **收邮件**：ImapClient（异步拉取）→ MailStore（SQLite 事务写入）→ MailListModel（UI 更新）
- **读邮件**：MailStore（SQLite 查询）→ MailListModel → MailPreview

---

## 面试准备：Qt 理解自评

> 以下是根据本项目开发过程中的对话，对你 Qt 掌握程度的评估。每个知识点分为 **理解层级** 和 **面试可讲点**。

### 理解层级定义
- **L3 能讲清楚原理**：能向面试官解释是什么、为什么、怎么用
- **L2 会正确使用**：项目中用了且用法正确
- **L1 有概念认知**：知道存在但可能写不出完整代码

---

### 一、Signal & Slot（信号槽）— L3 能讲清楚原理

**本项目体现：** 这是整个项目通信的骨架。`mailApp` 中连接了 9 对信号槽，从 UI 事件到网络回调全部靠它。

**你要能讲清楚的点：**
- **原理**：Qt 元对象系统通过 MOC 生成 `moc_*.cpp`，`connect()` 将信号函数的索引和槽函数的索引登记到 `QObjectPrivate::Connection` 链表
- **5 种连接方式**：函数指针（编译期检查，推荐）、SIGNAL/SLOT 宏（运行时字符串匹配，Qt4 遗留）、lambda、`QMetaObject::connectSlotsByName`、`QML` 绑定
- **连接类型**：`Qt::AutoConnection`（默认）— 同线程直接调用，跨线程 `QueuedConnection`（信号参数进事件队列）
- **线程安全**：emit 信号不需要锁，跨线程时 Qt 自动把参数拷贝到事件队列，接收者线程的事件循环里执行槽。**这里你可以举例**：本项目中 `SmtpClient::doSendEmail()` 在工作线程 emit `errorOccurred()`，`mailApp` 的槽在 UI 线程执行，Qt 自动处理了线程边界
- **常见坑**：信号和槽参数不能完全匹配时用 lambda 做适配；`connect` 返回值要检查（`QMetaObject::Connection`）；lambda 连接要注意捕获对象的生命周期

**面试话术：** "信号槽是 Qt 对观察者模式的实现，不是简单的回调。它的核心价值是松耦合——发送者不知道谁在监听。底层依赖 MOC 生成的 `qt_static_metacall` 函数做索引分发，跨线程时走事件队列而非直接调用。"

---

### 二、Model/View 架构 — L3 能讲清楚原理

**本项目体现：** `MailListModel : QAbstractListModel` 配合 `QListView`，自定义了 6 个 UserRole（FromRole, SubjectRole, DateRole, IsReadRole, FolderRole, EmailIdRole），还覆写了 `Qt::FontRole`（未读加粗）和 `Qt::ForegroundRole`（已读灰色）。

**你要能讲清楚的点：**
- **为什么需要 Model/View**：传统做法 `ui->listWidget->addItem()` 数据与 UI 耦合。Model/View 分离后，一份数据可以给多个 View（列表、表格、图表），换数据不需要改 View
- **Model 三件套**：`rowCount()` / `columnCount()`、`data(role)`、`index(row, col)`，对于可编辑的还要 `setData()` 和 `flags()`
- **Role 机制**：Qt 通过 `Qt::ItemDataRole` 让一个 cell 携带多种"面孔"——`DisplayRole` 是文字、`DecorationRole` 是图标、`FontRole` 是字体。自定义 Role 从 `Qt::UserRole` 开始编号
- **beginResetModel / endResetModel**：批量数据变更时通知 View 重绘，**本项目在 `setEmails()` 中用了这个**
- **相比 QStandardItemModel**：适合固定结构不需要自定义的简单场景，但本项目用 QStandardItemModel 做 FolderTree 是因为文件夹数量少、结构简单

**面试话术：** "Model/View 本质是 MVC 在 Qt 的落地，但不是标准 MVC——View 和 Controller 合并了，所以有时叫 MV 模式。Model 只管数据，不关心显示；View 通过 Role 向 Model 查询显示信息；Delegate 控制单个 cell 的渲染和编辑。这种架构的好处是单元测试时可以直接测 Model 不需要创建窗口。"

---

### 三、QtConcurrent + 异步模式 — L3 能讲清楚原理

**本项目体现：** `SmtpClient` 和 `ImapClient` 的核心设计——`QtConcurrent::run()` 在线程池执行阻塞 VMime 网络 I/O，`QFutureWatcher::finished` 信号通知 UI 线程完成。

**你要能讲清楚的点：**
- **为什么不用 QThread**：QThread 需要手动管理线程生命周期、moveToThread、quit/wait，代码量大且容易出错。QtConcurrent 是任务级别抽象，提交后忘掉，线程池管理全自动
- **线程池位置**：`QThreadPool::globalInstance()`，默认线程数 = `QThread::idealThreadCount()`
- **QFuture vs QFutureWatcher**：QFuture 是异步结果句柄（可查询状态、取返回值、取消）；QFutureWatcher 是 QFuture 的观察者（emit 信号）
- **跨线程信号**：`doSendEmail()` 在线程池执行时 emit `errorOccurred()`，Qt 检测到 receiver 在另一线程，自动用 `QueuedConnection`，信号参数拷贝到 receiver 的事件队列
- **本项目一个实际坑**：`onTaskFinished()` 和 `doSendEmail()` 的 catch 块可能双重触发信号——如果 `doSendEmail` catch 了异常 emit `errorOccurred`，然后 `onTaskFinished` 又 emit `emailSent`。需要一个 flag 或检查 future 是否有异常来避免

**面试话术：** "QtConcurrent 是 Qt 对 C++ 线程池的任务级封装。对比 QThread 的 run() 重写模式，QtConcurrent 更符合现代 C++ 的异步思维——描述你要做什么，不关心在哪个线程做。底层是 QThreadPool + QRunnable，用 std::function 包装任务。"

---

### 四、QObject 内存模型 — L3 能讲清楚原理

**本项目体现：** 所有服务（MailStore, AccountManager, SmtpClient, ImapClient）和所有 Widget 都 parent 到 `mailApp`，窗口关闭时自动销毁整棵树。

**你要能讲清楚的点：**
- **父子树**：QObject 通过 `QObjectPrivate::children` 链表维护孩子。父对象析构时遍历 children 逐一 delete
- **不是智能指针**：`QObject` 没有引用计数，父子关系提供的是**所有权**而非共享
- **正确姿势**：new 子对象时传 `this` 作 parent，不手动 delete。`Qt::WA_DeleteOnClose` 用于窗口——关闭时 `deleteLater()`
- **线程约束**：QObject 父子关系不能跨线程——子对象的 `thread()` 必须和父对象相同
- **QScopedPointer / std::unique_ptr 与 parent 的取舍**：如果一个 QObject 子对象需要提前销毁（不等到父对象析构），用 `deleteLater()` 而非裸 delete

**面试话术：** "QObject 父子树是 Qt 的 GC 机制。跟 Java/C# 的 GC 不同，它是确定性的——析构顺序完全可预测。代价是你必须保证子对象在堆上分配，且不能和 STL 智能指针混用。实际项目中，new 的时候传 parent，后面就不再操心释放。"

---

### 五、QSettings — L2 会正确使用

**本项目体现：** `AccountManager` 用 `beginReadArray`/`beginWriteArray` 读写账户列表。

**可讲点：** 了解 `QSettings` 的跨平台行为（macOS → plist, Win → Registry, Linux → ini），知道 `sync()` 的作用（强制立即写磁盘），知道 `IniFormat` vs `NativeFormat`。

---

### 六、QSqlDatabase / SQLite — L2 会正确使用

**本项目体现：** `MailStore` 建表、prepared statement（`INSERT OR REPLACE`）、事务（`transaction/commit/rollback`）、索引（`CREATE INDEX`）、命名连接避免冲突。

**可讲点：** 能说清楚为什么用 prepared statement（防注入 + 提高重复执行效率），为什么批量写入要套事务（SQLite 每 INSERT 一个隐式事务，1000 条就是 1000 次 fsync）。

---

### 七、跨线程信号与事件循环 — L2 会正确使用

**本项目体现：** 工作线程 emit 信号，UI 线程的 slot 接收。这是跨线程通信的标准模式。

**可讲点：** 理解 `QueuedConnection` 的机制——信号参数被深拷贝到 `QMetaCallEvent`，投递到接收者线程的事件队列，由事件循环 `exec()` 取出执行。理解为什么 `QThread::run()` 里直接 emit 信号不会被送到 UI 线程——因为接收者不在事件循环里。

---

### 八、QMainWindow / QDialog / QWidget 体系 — L2 会正确使用

**本项目体现：** `mailApp` 和 `ComposeWindow` 用 `QMainWindow`（有 MenuBar/ToolBar/StatusBar 需求），`SettingsDialog` 用 `QDialog`（模态对话框），`MailPreview` 用 `QWidget`（纯容器）。

**理解正确：** 你知道什么时候该用哪个基类，而不是一律 QWidget。

---

### 九、协议级调试能力 — 加分项

**对话中体现：** 你主动用 Wireshark/tcpdump 抓包，通过实际网络流量发现 "客户端根本没发 AUTH" 的 bug。这超出了纯 Qt 范畴，是后端/协议层的调试思维。

**可讲点：** "遇到网络问题，我不会只在代码里打日志。OSI 分层去定位——先抓包看 TCP 握手是否成功，再看 TLS 握手是否完成，最后看应用层 SMTP 命令是否符合 RFC。"

---

### 十、你还可以准备的面试问题

以下是典型 Qt 客户端面试题，基于你的代码你能回答：

| 问题 | 你的答案素材 |
|------|------------|
| "Qt 信号槽怎么实现的？" | MOC 生成 `qt_static_metacall`，每个信号有索引号，connect 登记在 QObjectPrivate 链表 |
| "Model/View 和 QListWidget 的区别？" | 数据分离 / 一份数据多 View / 自定义 Role / 大数据量性能（View 只渲染可见项） |
| "怎么在 Qt 里做异步不阻塞 UI？" | QtConcurrent::run + QFutureWatcher，具体讲 smtpclient 的设计 |
| "QObject 内存怎么管理？" | 父子树 + WA_DeleteOnClose + deleteLater + 不能和 STL 智能指针混用 |
| "QSettings 存密码安全吗？" | 不，明文。所以我们做了 AES 加密——机器 ID 派生密钥，存在本设备解不开 |
| "跨线程信号槽怎么工作？" | AutoConnection 检测线程 ID，跨线程走 QueuedConnection——参数深拷贝 + 事件队列 |
| "做过什么性能优化？" | MailStore batch import 用事务包裹；beginResetModel/endResetModel 批量更新；QFutureWatcher 复用而非每次 new |

---

### 总体评价

你的 Qt 掌握程度：**中级偏上，能独立从零搭建一个邮件客户端。**

**优势：**
- 对信号槽、Model/View、异步并发的理解是**原理级**的，不是只会照着教程写
- 架构意识好——知道分层（UI/Service/Network/Data），而不是把所有代码塞一个类里
- 有调试思维——会抓包、会读第三方库源码（VMime 的 SMTPConnection.cpp）定位问题
- 可以独立完成从 CMake 构建到运行的一整套流程

**面试建议：**
- 上面列的原理问题要能**用自己的话讲出来**，不要背书
- 如果被问到"遇到过什么坑"，**讲本项目的双重 emit 问题和 VMime 默认不启用 AUTH 的问题**——这比泛泛而谈的 bug 有说服力
- 如果被要求现场写代码，侧重展示信号槽连接 + Model 的自定义 Role
