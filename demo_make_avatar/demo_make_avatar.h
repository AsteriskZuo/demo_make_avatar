#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_demo_make_avatar.h"

class Task;
class demo_make_avatar : public QMainWindow
{
	Q_OBJECT

public:
	demo_make_avatar(QWidget *parent = Q_NULLPTR);
	~demo_make_avatar();

private:
	Ui::demo_make_avatarClass ui;
	Task* mpTask;
	QList<QByteArray> mTaskList;

private slots:
	void selectFile(bool);
	void startTask(bool);
	void stopTask(bool);
	void clearTask(bool);
	void printLog(const QString& content);

signals:
	void sigStartTask(const QString& url, const QSize& sz);//开始任务
	void siogStopTask();//停止任务
	void sigClearTask();//清除所有任务

};
