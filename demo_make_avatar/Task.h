#pragma once
#ifndef TASK_H
#define TASK_H

#include "base.h"
using namespace demo;

#include <QString>
#include <QThread>
#include <QQueue>
#include <QHash>
#include <QSharedPointer>
#include <QMutex>
#include <QWaitCondition>

struct TaskData 
{
	TaskData();
	inline void reset();
	int id;
	QString url;//下载地址
	QString orgurl;//下载地址
	QString fileName;//保存到本地的文件名
	QSize size;//头像大小
	int state;//任务状态 0.初始化 1.开始 2.下载 3.制作 4.完成(41成功 42下载失败 43制作失败)
};

struct AvatarData 
{
	QSize size;//头像尺寸
	QSharedPointer<QImage> spImage;//头像
};

class Task : public QThread
{
	Q_OBJECT
public:
	SINGLETON_DEFINE(Task);
	Task();
	virtual ~Task();

public:

protected:
	//************************************
	// Method:    download
	// FullName:  Task::download
	// Access:    protected 
	// Returns:   int 0.成功 1.失败
	// Qualifier: 下载头像文件
	// Parameter: const int & id 任务编号
	//************************************
	int download(const int& id);

	//************************************
	// Method:    makeAvatar
	// FullName:  Task::makeAvatar
	// Access:    protected 
	// Returns:   int 0.成功 1.失败
	// Qualifier: 制作头像
	// Parameter: const int & id 任务编号
	//************************************
	int makeAvatar(const int& id);

	virtual void run();

private:
	void changedState(int state);
	QImage getImage(const QString& name, const QSize& sz);
	void addImage(const QImage& avatar);

private:
	QMutex mMutex;
	QWaitCondition mWait;

	bool mRequestQuit;
	bool mManualStop;

	QQueue<TaskData> mTaskList;//等待的任务列表
	TaskData mCurrentTask;//当前执行的任务
	QQueue<TaskData> mErrorTaskList;//失败的任务列表
	QHash<QString, QList<AvatarData>> mAvatar;//头像

public slots:
	void startTask(const QString& url, const QSize& sz);//开始任务
	void stopTask();//停止任务
	void clearTask();//清除所有任务

signals:
	void cancelTask();//取消任务
	void printLog(const QString& content);
};

#endif //TASK_H
