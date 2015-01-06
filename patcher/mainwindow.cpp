#include "mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <stdexcept>

#include "FileLoader.h"
#include "ImportsTableModel.h"
#include "StubMaker.h"

QString  SITE_LINK = "http://hasherezade.net/IAT_patcher";

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent), m_replacementsDialog(NULL),
    infoModel(NULL), m_ExeSelected(NULL), customMenu(NULL), functionsMenu(NULL)
{
    m_ui.setupUi(this);
    initReplacementsDialog();

    makeCustomMenu();
    makeFunctionsMenu();
    makeFileMenu();

    this->infoModel = new InfoTableModel(m_ui.outputTable);
    infoModel->setExecutables(&m_exes);
/*
    QSortFilterProxyModel *m = new QSortFilterProxyModel(this);
    m->setDynamicSortFilter(true);
    m->setSourceModel(infoModel);

	m_ui.outputTable->setModel(m);
    m_ui.outputTable->setSortingEnabled(true);
*/
    m_ui.outputTable->setModel(infoModel);
    m_ui.outputTable->setSortingEnabled(false);
    m_ui.outputTable->horizontalHeader()->setStretchLastSection(false);
    m_ui.outputTable->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
	m_ui.outputTable->verticalHeader()->show();

	m_ui.outputTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui.outputTable->setSelectionMode(QAbstractItemView::SingleSelection);
	m_ui.outputTable->setEditTriggers(QAbstractItemView::NoEditTriggers);


    m_ui.statusBar->addPermanentWidget(&urlLabel);
    urlLabel.setTextFormat(Qt::RichText);
	urlLabel.setTextInteractionFlags(Qt::TextBrowserInteraction);
	urlLabel.setOpenExternalLinks(true);
    urlLabel.setText("<a href=\""+ SITE_LINK +"\">"+SITE_LINK+"</a>");

	this->setAcceptDrops(true);

    this->impModel = new ImportsTableModel(m_ui.outputTable);

    this->m2 = new QSortFilterProxyModel(this);
    m2->setDynamicSortFilter(true);
    m2->setSourceModel(impModel);

	m_ui.importsTable->setModel(m2);
    m_ui.importsTable->setSortingEnabled(true);
    m_ui.importsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui.importsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ui.importsTable->horizontalHeader()->setResizeMode(2, QHeaderView::Stretch);


    connect( m_ui.filterLibEdit, SIGNAL( textChanged (const QString &)), this, SLOT( filterLibs(const QString &)) );
    connect( m_ui.filterProcEdit, SIGNAL( textChanged (const QString &)), this, SLOT( filterFuncs(const QString &)) );
    m_ui.outputTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ui.outputTable, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customMenuRequested(QPoint)));

    m_ui.importsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ui.importsTable, SIGNAL(customContextMenuRequested(QPoint)), SLOT(functionsMenuRequested(QPoint)));

	connect( m_ui.outputTable->selectionModel(), SIGNAL( currentRowChanged(QModelIndex,QModelIndex)), this, SLOT( rowChangedSlot(QModelIndex,QModelIndex) ) );
	connect( this, SIGNAL( exeSelected(ExeHandler*)), impModel, SLOT( setExecutable(ExeHandler*) ) );

    connect( this, SIGNAL( exeUpdated(ExeHandler*)), impModel, SLOT( setExecutable(ExeHandler*) ) );
    connect( this, SIGNAL( exeUpdated(ExeHandler*)), infoModel, SLOT( onExeListChanged() ) );

    connect( &this->exeController, SIGNAL( exeUpdated(ExeHandler*)), infoModel, SLOT( onExeListChanged() ) );

    connect(&m_LoadersCount, SIGNAL(counterChanged()), this, SLOT(loadingStatusChanged() ));
    connect(this, SIGNAL(hookRequested(ExeHandler* )), this, SLOT(onHookRequested(ExeHandler* ) ));
    connect(infoModel, SIGNAL(hookRequested(ExeHandler* )), this, SLOT(onHookRequested(ExeHandler* ) ));

    connect(this, SIGNAL(thunkSelected(offset_t)), this, SLOT(setThunkSelected(offset_t)) );
}

MainWindow::~MainWindow()
{
    clear();
}

void MainWindow::initReplacementsDialog()
{
    this->m_replacementsDialog = new QDialog(this);
    this->m_uiReplacements = new Ui_Replacements();
    this->m_uiReplacements->setupUi(m_replacementsDialog);

    connect(this->m_uiReplacements->okCancel_buttons, SIGNAL(rejected()), this->m_replacementsDialog, SLOT(hide()));
    connect(this->m_uiReplacements->okCancel_buttons, SIGNAL(accepted()), this, SLOT(setReplacement()));
}

void MainWindow::filterLibs(const QString &str)
{
    m2->setFilterRegExp(QRegExp(str, Qt::CaseInsensitive, QRegExp::FixedString));
    m2->setFilterKeyColumn(1);
}

void MainWindow::filterFuncs(const QString &str)
{
    m2->setFilterRegExp(QRegExp(str, Qt::CaseInsensitive, QRegExp::FixedString));
    m2->setFilterKeyColumn(2);
}

void MainWindow::loadingStatusChanged()
{
    size_t count = m_LoadersCount.getCount();
    if (count == 0) {
        this->m_ui.statusBar->showMessage("");
        return;
    }
    QString ending = "s";
    if (count == 1) ending = "";
    this->m_ui.statusBar->showMessage("Loading: " + QString::number(count) + " file" + ending);
}

void MainWindow::dropEvent(QDropEvent* ev)
{
	QList<QUrl> urls = ev->mimeData()->urls();
	QList<QUrl>::Iterator urlItr;
	QCursor cur = this->cursor();
	this->setCursor(Qt::BusyCursor);

	for (urlItr = urls.begin() ; urlItr != urls.end(); urlItr++) {
        QString fileName = urlItr->toLocalFile();
		if (fileName == "") continue;
        if (parse(fileName)) {
		    m_ui.fileEdit->setText(fileName);
        }
    }
	this->setCursor(cur);
}

void MainWindow::on_pushButton_clicked()
{
    return openExe();
}

void MainWindow::openExe()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open executable"),
        QDir::homePath(),
        "Exe Files (*.exe);;DLL Files (*.dll);;All files (*.*)"
        );

    if (fileName != "") {
        this->parse(fileName);
    }
}

void MainWindow::removeExe(ExeHandler* exe)
{
    selectExe(NULL);
    this->m_exes.removeExe(exe);
    delete exe;
    exe = NULL;
}

void MainWindow::on_reloadButton_clicked()
{
    QStringList files = this->m_exes.listFiles();
    clear();
    QStringList::iterator itr;
    for (itr = files.begin(); itr != files.end(); itr++) {
        QString fileName = *itr;
        this->parse(fileName);
    }
}

void MainWindow::reloadExe(ExeHandler* exe)
{
    QString fName = "";
    if (exe && exe->getExe())
        fName = exe->getExe()->getFileName();
    this->removeExe(exe);
    this->parse(fName);
}

void MainWindow::clear()
{
    selectExe(NULL);
    this->m_exes.clear();
}

void MainWindow::on_hookButton_clicked()
{
    emit hookRequested(this->m_ExeSelected);
}

void MainWindow::addExeAction(QMenu *customMenu, QString text, ExeController::EXE_ACTION a)
{
    QAction *hook = new QAction(text, customMenu);
    hook->setData(a);
    customMenu->addAction(hook);
    connect(hook, SIGNAL(triggered()), this, SLOT(takeAction()));
}

void MainWindow::makeCustomMenu()
{
    this->customMenu = new QMenu(this);

    addExeAction(customMenu, "Hook", ExeController::ACTION_HOOK);
    addExeAction(customMenu, "Save as...", ExeController::ACTION_SAVE);
    addExeAction(customMenu, "Reload", ExeController::ACTION_RELOAD);
    addExeAction(customMenu, "Unload", ExeController::ACTION_UNLOAD);
}

void MainWindow::makeFunctionsMenu()
{
    this->functionsMenu = new QMenu(this);
    QAction *settingsAction = new QAction("Define replacement", functionsMenu);
    connect(settingsAction, SIGNAL(triggered()), this->m_replacementsDialog, SLOT(show()));
    functionsMenu->addAction(settingsAction);
}

void MainWindow::makeFileMenu()
{
    QMenu *menu = this->m_ui.menuFile;

    QAction *openAction = new QAction("Open executable", menu);
    connect(openAction, SIGNAL(triggered()), this, SLOT(openExe()));
    menu->addAction(openAction);

    addExeAction(menu, "Hook selected", ExeController::ACTION_HOOK);
    addExeAction(menu, "Save selected as...", ExeController::ACTION_SAVE);
}

void MainWindow::customMenuRequested(QPoint pos)
{
    QTableView *table = this->m_ui.outputTable;

    QModelIndex index = table->indexAt(pos);
    if (index.isValid() == false) return;

    this->customMenu->popup(table->viewport()->mapToGlobal(pos));
}

void MainWindow::functionsMenuRequested(QPoint pos)
{
    QTableView *table = this->m_ui.importsTable;
    if (table == NULL ) return;
    if (this->m_ExeSelected == NULL) return;

    QModelIndex index = table->indexAt(pos);
    if (index.isValid() == false) return;

    m_ThunkSelected = this->impModel->selectedIndexToThunk(index);
    emit thunkSelected(m_ThunkSelected);

    FuncDesc replName = this->m_ExeSelected->getReplAt(m_ThunkSelected);
    //todo: on show?
    QString libName;
    QString funcName;
    FuncUtil::parseFuncDesc(replName, libName, funcName);

    this->m_uiReplacements->libraryEdit->setText(libName);
    this->m_uiReplacements->functionEdit->setText(funcName);

    this->functionsMenu->popup(table->viewport()->mapToGlobal(pos));
}

void MainWindow::setThunkSelected(offset_t thunk)
{
    QString thunkStr = QString::number(thunk, 16);
    QString libName = this->m_ExeSelected->m_FuncMap.thunkToLibName(thunk);
    QString funcName = this->m_ExeSelected->m_FuncMap.thunkToFuncName(thunk);

    this->m_uiReplacements->thunkLabel->setText(thunkStr);
    this->m_uiReplacements->libToReplaceLabel->setText(libName+"."+funcName);
}


void MainWindow::setReplacement()
{
    QString libName = this->m_uiReplacements->libraryEdit->text();
    QString funcName = this->m_uiReplacements->functionEdit->text();
    QString substName = "";

    if (libName.length() != 0 && funcName.length() != 0) {
        substName = libName + "." + funcName;
    }
    if (this->m_ExeSelected->m_Repl.getAt(m_ThunkSelected) == substName) return;

    if (this->m_ExeSelected->hook(m_ThunkSelected, substName) == false) {
        QMessageBox::warning(NULL, "Error", "Invalid replacement definition!");
    } else {
        this->m_replacementsDialog->hide();
    }
}


void MainWindow::takeAction()
{
    QAction *action = qobject_cast<QAction *>(sender());

    if (action->data() == ExeController::ACTION_HOOK) {
        emit hookRequested(this->m_ExeSelected);
        return;
    }
    if (action->data() == ExeController::ACTION_SAVE) {
        exeController.onSaveRequested(this->m_ExeSelected);
        return;
    }
    if (action->data() == ExeController::ACTION_UNLOAD) {
        this->removeExe(this->m_ExeSelected);
        return;
    }
    if (action->data() == ExeController::ACTION_RELOAD) {
        this->reloadExe(this->m_ExeSelected);
        return;
    }
}

void MainWindow::onHookRequested(ExeHandler* exeHndl)
{
    StubSettings settings;
    settings.setAddNewSection(this->m_ui.actionAdd_new_section->isChecked());
    settings.setReuseImports(this->m_ui.actionAdd_imports->isChecked());
    QString settingsStr = "Settings: ";
    if (settings.getAddNewSection()) {
        settingsStr += "add new section ;";
    }
    if (settings.getReuseImports()) {
        settingsStr += "reuse imports";
    }
    this->m_ui.statusBar->showMessage(settingsStr);

    exeController.onHookRequested(exeHndl, settings);
}

void MainWindow::on_saveButton_clicked()
{
    exeController.onSaveRequested(this->m_ExeSelected);
}

void MainWindow::onFileLoaded(AbstractByteBuffer* buf)
{
    if (buf == NULL) {
        QMessageBox::warning(NULL,"Error!", "Cannot load the file!");
        delete buf;
        return;
    }

    ExeFactory::exe_type exeType = ExeFactory::findMatching(buf);
    if (exeType == ExeFactory::NONE) {
        QMessageBox::warning(NULL,"Cannot parse", "Type not supported\n");
        delete buf;
        return;
    }
    try {
        printf("Parsing executable...\n");
        Executable *exe = ExeFactory::build(buf, exeType);
        ExeHandler *exeHndl = new ExeHandler(buf, exe);
        if (exeHndl == NULL) throw CustomException("Cannot create handle!");
        StubMaker::fillHookedInfo(exeHndl);
        m_exes.addExe(exeHndl);

    } catch (CustomException &e) {
        QMessageBox::warning(NULL, "ERROR", e.getInfo());
        return;
    }
	return;
}

void MainWindow::onLoaderThreadFinished()
{
    delete QObject::sender();
    m_LoadersCount.dec();
    this->repaint();
}

void MainWindow::rowChangedSlot(QModelIndex curr, QModelIndex prev)
{
    size_t index = curr.row();
    size_t prevIndex = prev.row();
    //QVariant data = curr.data();

    ExeHandler *exe = this->m_exes.at(index);
    selectExe(exe);
}

bool MainWindow::parse(QString &fileName)
{
    if (fileName == "") return false;

    QString link = QFile::symLinkTarget(fileName);
	if (link.length() > 0) fileName = link;

    bufsize_t maxMapSize = FILE_MAXSIZE;
    try {
        FileView fileView(fileName, maxMapSize);
        ExeFactory::exe_type exeType = ExeFactory::findMatching(&fileView);
        if (exeType == ExeFactory::NONE) {
            QMessageBox::warning(NULL,"Cannot parse", "Type not supported\n");
            return false;
        }

        FileLoader *loader = new FileLoader(fileName);
		QObject::connect(loader, SIGNAL( loaded(AbstractByteBuffer*) ), this, SLOT( onFileLoaded(AbstractByteBuffer*) ) );
		QObject::connect(loader, SIGNAL(finished()), this, SLOT( onLoaderThreadFinished() ) );
		//printf("Thread started...\n");
        m_LoadersCount.inc();
        loader->start();

    } catch (CustomException &e) {
        QMessageBox::warning(NULL, "ERROR", e.getInfo());
        return false;
    }
    return true;
}

void MainWindow::info()
{
	int ret = 0;
	int count = 0;
	QPixmap p(":/favicon.ico");
	QString msg = "<b>IAT Patcher</b> - tool for persistent IAT hooking<br/>";
	msg += "author: hasherezade<br/><br/>";
	msg += "THIS TOOL IS PROVIDED \"AS IS\" WITHOUT WARRANTIES OF ANY KIND. <br/>\
        Use it at your own risk and responsibility.<br/>\
        Only for research purpose. Do not use it to break the law!";

	QMessageBox msgBox;
	QLabel urlLabel;
	urlLabel.setTextFormat(Qt::RichText);

	urlLabel.setTextInteractionFlags(Qt::TextBrowserInteraction);
	urlLabel.setOpenExternalLinks(true);
	msgBox.setWindowTitle("Info");

	urlLabel.setText("<a href=\"" + SITE_LINK + "\">More...</a>");

	msgBox.layout()->addWidget(&urlLabel);

	msgBox.setText(msg);
	msgBox.setAutoFillBackground(true);
	msgBox.setIconPixmap(p);
	msgBox.setStandardButtons(QMessageBox::Ok);
	msgBox.setDefaultButton(QMessageBox::Ok);
	msgBox.exec();
}