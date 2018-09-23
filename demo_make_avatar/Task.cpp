#include "Task.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QDebug>

static int sid = 0;

TaskData::TaskData()
	: state(0)
	, id(++sid)
{
}

void TaskData::reset()
{
	id = 0;
	url.clear();
	orgurl.clear();
	fileName.clear();
	state = 0;
}



Task::Task()
	: mRequestQuit(false)
	, mManualStop(false)
{
	const QString& localFileDir = GetSavePath();
	QDir dir(localFileDir);
	if (!dir.exists())
	{
		int ret  = dir.mkdir(localFileDir);
		qDebug() << __FUNCTION__ << ret;
	}
}


Task::~Task()
{
}

void Task::startTask(const QString& url, const QSize& sz)
{
	TaskData d;
	d.url = url;
	d.orgurl = url;
	d.fileName = GetFileName(url);
	d.size = sz;
	d.state = 0;
	mTaskList.enqueue(d);

	mMutex.lock();
	mManualStop = false;
	mWait.wakeAll();
	mMutex.unlock();
}
void Task::stopTask()
{
	mMutex.lock();
	mManualStop = true;
	mMutex.unlock();
}
void Task::clearTask()
{
	mTaskList.clear();
	mErrorTaskList.clear();
	mAvatar.clear();
	mCurrentTask.reset();
}

int Task::download(const int& id)
{
loop:


	QNetworkRequest request;
	if (mCurrentTask.url.startsWith("https://"))
	{
		QSslConfiguration config;
		config.setPeerVerifyMode(QSslSocket::VerifyNone);
		request.setSslConfiguration(config);
	}
	request.setUrl(mCurrentTask.url);

	QNetworkAccessManager manager;
	QNetworkReply	*pReply = manager.get(request);
	ReplyTimeout *pTimeout = new ReplyTimeout(pReply, 3000);
	connect(this, &Task::cancelTask, pReply, &QNetworkReply::abort, Qt::QueuedConnection);

	QEventLoop loop;
	connect(pReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	QNetworkReply::NetworkError errorCode = pReply->error();
	if (pReply->error() == QNetworkReply::NoError)
	{
		int statusCode = pReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (statusCode == 301 || statusCode == 302) {
			qDebug() << "statusCode: " << statusCode << " path: " << mCurrentTask.url;
			mCurrentTask.url = pReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();
			qDebug() << "statusCode: " << statusCode << " new path: " << mCurrentTask.url;
			goto loop;
		}
	}

	int ret = 0;
	if (errorCode == QNetworkReply::NoError && !pTimeout->isTimeout())
	{
		QByteArray buffer = pReply->readAll();
		if (0 < buffer.size())
		{
			const QString& localFilePath = GetSavePath() + QDir::separator() + mCurrentTask.fileName;
			QFile file(localFilePath);
			if (file.open(QIODevice::ReadWrite | QIODevice::Truncate) && file.isWritable())
			{
				file.seek(0);
				file.write(buffer);
				file.close();
				ret = 0;
			}
			else ret = 1;
		}
		else ret = 2;
	}
	else ret = 3;
	if (0 == ret)
	{
		changedState(2);
	}
	else
	{
		qDebug() << __FUNCTION__ << errorCode << pReply->errorString() << pTimeout->isTimeout() << ret;
		changedState(42);
		mErrorTaskList.enqueue(mCurrentTask);
	}
	emit printLog(QString("download: id=%1; url=%2; name=%3; result=%4;").arg(mCurrentTask.id).arg(mCurrentTask.url).arg(mCurrentTask.fileName).arg(QString::number(ret)));
	return ret;
}
int Task::makeAvatar(const int& id)
{
	int ret = 0;
	const QString& localFilePath = GetSavePath() + QDir::separator() + mCurrentTask.fileName;
	QImage&& image = MakeRoundAvatar(localFilePath, mCurrentTask.size);
	if (!image.isNull())
	{
		ret = 0;
		changedState(3);
		addImage(image);
		emit taskResult(image);
	}
	else
	{
		ret = 1;
		qDebug() << __FUNCTION__ << ret;
		changedState(43);
		mErrorTaskList.enqueue(mCurrentTask);
	}
	emit printLog(QString("makeAvatar: id=%1; url=%2; name=%3; result=%4;").arg(mCurrentTask.id).arg(mCurrentTask.url).arg(mCurrentTask.fileName).arg(QString::number(ret)));
	return ret;
}

void Task::run()
{
	for (;;)
	{
		if (mRequestQuit)
			break;

		if (mManualStop
			|| 0 == mTaskList.size())
		{
			mMutex.lock();
			mWait.wait(&mMutex);
			mMutex.unlock();
		}

		mCurrentTask = mTaskList.dequeue();
		changedState(1);

		do
		{
			if (0 != download(mCurrentTask.id))
			{
				break;
			}
			if (0 != makeAvatar(mCurrentTask.id))
			{
				break;
			}
			changedState(41);
		} while (false);


	}
}

void Task::changedState(int state)
{
	mCurrentTask.state = state;
}

QImage Task::getImage(const QString& name, const QSize& sz)
{
	QImage avatar;
	if (mAvatar.contains(name))
	{
		const QList<AvatarData>& list = mAvatar[name];
		for (const AvatarData& a : list)
		{
			if (sz == a.size)
			{
				avatar = *(a.spImage.data());
				break;
			}
		}
	}
	return std::move(avatar);
}

void Task::addImage(const QImage& avatar)
{
	if (mAvatar.contains(mCurrentTask.fileName))
	{
		QList<AvatarData>& list = mAvatar[mCurrentTask.fileName];
		bool isExist = false;
		for (const AvatarData& a : list)
		{
			if (mCurrentTask.size == a.size)
			{
				*(a.spImage.data()) = avatar;
				isExist = true;
				break;
			}
		}
		if (!isExist)
		{
			AvatarData a;
			a.size = mCurrentTask.size;
			a.spImage.reset(new QImage(avatar));
			list.append(a);
		}
	}
	else
	{
		AvatarData a;
		a.size = mCurrentTask.size;
		a.spImage.reset(new QImage(avatar));
		QList<AvatarData> list;
		list.append(a);
		mAvatar.insert(mCurrentTask.fileName, list);
	}
}