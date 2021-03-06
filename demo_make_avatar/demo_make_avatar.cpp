#include "demo_make_avatar.h"
#include <QtWidgets>
#include <QDebug>
#include "Task.h"

demo_make_avatar::demo_make_avatar(QWidget *parent)
	: QMainWindow(parent)
	, mpTask(nullptr)
{
	ui.setupUi(this);
	connect(ui.toolButton_select_file, &QToolButton::clicked, this, &demo_make_avatar::selectFile);
	connect(ui.pushButton_start, &QToolButton::clicked, this, &demo_make_avatar::startTask);
	connect(ui.pushButton_stop, &QToolButton::clicked, this, &demo_make_avatar::stopTask);
	connect(ui.pushButton_clear, &QToolButton::clicked, this, &demo_make_avatar::clearTask);

	mpTask = new Task;
	connect(this, &demo_make_avatar::sigStartTask, mpTask, &Task::startTask, Qt::QueuedConnection);
	connect(this, &demo_make_avatar::siogStopTask, mpTask, &Task::stopTask, Qt::QueuedConnection);
	connect(this, &demo_make_avatar::sigClearTask, mpTask, &Task::clearTask, Qt::QueuedConnection);
	connect(mpTask, &Task::printLog, this, &demo_make_avatar::printLog, Qt::QueuedConnection);
	connect(mpTask, &Task::taskResult, this, &demo_make_avatar::taskResult, Qt::QueuedConnection);
	mpTask->start();
}

demo_make_avatar::~demo_make_avatar()
{
	if (mpTask->isRunning())
	{
		mpTask->quit();
		mpTask->wait();
		delete mpTask; mpTask = nullptr;
	}
}

void demo_make_avatar::selectFile(bool)
{
	QString taskfile = QFileDialog::getOpenFileName(this, tr("open file"), "", tr("task file (*.txt)"));
	if (!taskfile.isEmpty())
	{
		ui.lineEdit->setText(taskfile);
		QFile file(taskfile);
		if (file.open(QIODevice::ReadOnly))
		{
			QByteArray contentb = file.readAll();
			QString contents = QString::fromUtf8(contentb);
			mTaskList = contents.split(';', QString::SkipEmptyParts);
		}
	}
}

void demo_make_avatar::startTask(bool)
{
	ui.pushButton_start->setEnabled(false);
	ui.pushButton_clear->setEnabled(false);

	for (const QString& a : mTaskList)
	{
		if (!a.isEmpty())
		{
			QString url = a;
			url = url.trimmed();
			emit sigStartTask(url, QSize(30, 30));
		}
	}
}

void demo_make_avatar::stopTask(bool)
{
	ui.pushButton_start->setEnabled(true);
	ui.pushButton_clear->setEnabled(true);

	emit siogStopTask();
}

void demo_make_avatar::clearTask(bool)
{
	ui.lineEdit->clear();
	ui.plainTextEdit->clear();
	ui.textEdit_avatar->clear();

	emit sigClearTask();
}

void demo_make_avatar::printLog(const QString& content)
{
	ui.plainTextEdit->appendPlainText(content);
}

void demo_make_avatar::taskResult(const QImage& avatar)
{
	//////////////////////////////////////////////////////////////////////////TEST START

	//QFile file("test");
	//if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	//{
	//	QDataStream s(&file);
	//	s << avatar;
	//	file.flush();
	//	file.close();
	//}

	//////////////////////////////////////////////////////////////////////////TEST END

	if (!avatar.isNull())
	{
		QTextCursor cursor = ui.textEdit_avatar->textCursor();
		cursor.insertImage(avatar);

		//////////////////////////////////////////////////////////////////////////TEST START

		//QString imageName = QString::number(avatar.cacheKey());
		//QTextDocument *document = ui.textEdit_avatar->document();
		//QImage avatar_copy = document->resource(QTextDocument::ImageResource, QUrl(imageName)).value<QImage>();
		//cursor.insertImage(avatar);
		//avatar.save("d:\\test3", "png");

		//////////////////////////////////////////////////////////////////////////TEST END
	}
}
