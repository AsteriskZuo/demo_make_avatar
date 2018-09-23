#include "Task.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QThreadPool>
#include <QDebug>



static int sid = 0;
const static quint64 smaxsize = 1024 * 1024 * 1024;

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
	, mThreshold(0)
{
	const QString& localFileDir = GetSavePath();
	QDir dir(localFileDir);
	if (!dir.exists())
	{
		int ret  = dir.mkdir(localFileDir);
		qDebug() << __FUNCTION__ << ret;
	}

	mThreshold = 1024 * 1024 * 10;
	mMaxTaskCount = 5;
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
stmtloop:


	QNetworkRequest request;
	if (mCurrentTask.url.startsWith("https://"))
	{
		QSslConfiguration config;
		config.setPeerVerifyMode(QSslSocket::VerifyNone);
		request.setSslConfiguration(config);
	}
	request.setUrl(mCurrentTask.url);


	bool isbigfile = false;

	if (true)
	{
		//先获取头文件
		QNetworkAccessManager _manager;
		QNetworkReply* _pReply = _manager.head(request);
		ReplyTimeout *_pTimeout = new ReplyTimeout(_pReply, 3000);
		connect(this, &Task::cancelTask, _pReply, &QNetworkReply::abort, Qt::QueuedConnection);
		QEventLoop _loop;
		connect(_pReply, &QNetworkReply::finished, &_loop, &QEventLoop::quit);
		_loop.exec();
		QNetworkReply::NetworkError errorCode = _pReply->error();
		if (errorCode == QNetworkReply::NoError && !_pTimeout->isTimeout())
		{
			int statusCode = _pReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if (statusCode == 301 || statusCode == 302) {
				qDebug() << "statusCode: " << statusCode << " path: " << mCurrentTask.url;
				mCurrentTask.url = _pReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();
				qDebug() << "statusCode: " << statusCode << " new path: " << mCurrentTask.url;
				goto stmtloop;
			}

			QString&& length = _pReply->header(QNetworkRequest::ContentLengthHeader).toString();
			QString&& type = _pReply->header(QNetworkRequest::ContentTypeHeader).toString();
			QString&& cookie = _pReply->header(QNetworkRequest::CookieHeader).toString();
			QString&& disp = _pReply->header(QNetworkRequest::ContentDispositionHeader).toString();
			QString&& agen = _pReply->header(QNetworkRequest::UserAgentHeader).toString();
			QString&& svr = _pReply->header(QNetworkRequest::ServerHeader).toString();
			qDebug() << __FUNCTION__ << "ContentLengthHeader:" << length
				<< "ContentTypeHeader:" << type
				<< "CookieHeader:" << cookie
				<< "ContentDispositionHeader:" << disp
				<< "UserAgentHeader:" << agen
				<< "ServerHeader:" << svr;
			
			if (mThreshold < length.toULongLong())
			{
				isbigfile = true;
			}
		}

	}

	int ret = 0;
	QString errorContent;
	if (isbigfile)
	{
		//do big file
		doBigFile(id);
		ret = 4;
	}
	else
	{
		QNetworkAccessManager manager;
		QNetworkReply	*pReply = manager.get(request);
		ReplyTimeout *pTimeout = new ReplyTimeout(pReply, 3000);
		connect(this, &Task::cancelTask, pReply, &QNetworkReply::abort, Qt::QueuedConnection);

		QEventLoop loop;
		connect(pReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
		loop.exec();

		QNetworkReply::NetworkError errorCode = pReply->error();
		errorContent = pReply->errorString();
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
	}

	emit printLog(QString("%5: id=%1; url=%2; name=%3; result=%4;")
		.arg(mCurrentTask.id)
		.arg(mCurrentTask.url)
		.arg(mCurrentTask.fileName)
		.arg(QString::number(ret))
		.arg(__FUNCTION__)
		.arg(errorContent));
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
	emit printLog(QString("%5: id=%1; url=%2; name=%3; result=%4;")
		.arg(mCurrentTask.id)
		.arg(mCurrentTask.url)
		.arg(mCurrentTask.fileName)
		.arg(QString::number(ret))
		.arg(__FUNCTION__));
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

void Task::doBigFile(const int& id)
{
	//耗时较长的任务
	BigTask* pBigTask = new BigTask(mCurrentTask);
	if (mMaxTaskCount < mBigWorkingTaskList.size())
	{
		mBigWaitTaskList.enqueue(pBigTask);
	}
	else
	{
		if (mBigWorkingTaskList.contains(id))
		{
			qWarning() << __FUNCTION__ << "big task is queue." << id;
			mBigWorkingTaskList.remove(id);
		}
		doBigTask(pBigTask);
	}
}

void Task::doBigTask(BigTask* pBigTask)
{
	Q_ASSERT(pBigTask);
	mBigWorkingTaskList.insert(pBigTask->getTaskInfo().id, pBigTask);
	connect(this, &Task::startBigTask, pBigTask, &BigTask::startBigTask, Qt::QueuedConnection);
	connect(this, &Task::stopBigTask, pBigTask, &BigTask::stopBigTask, Qt::QueuedConnection);
	connect(pBigTask, &BigTask::retBigTaskResult, this, &Task::retBigTaskResult, Qt::QueuedConnection);
	connect(pBigTask, &BigTask::printLog, this, &Task::printLog, Qt::QueuedConnection);
	pBigTask->setAutoDelete(false);
	pBigTask->startBigTask();
}

void Task::retBigTaskResult(const int& id, const bool& result, const QImage& avatar)
{
	if (mBigWorkingTaskList.contains(id))
	{
		TaskData&& info = mBigWorkingTaskList[id]->getTaskInfo();
		if (result)
		{
			QImage _avatar(avatar);
			addImage(_avatar);
			emit taskResult(_avatar);

			BigTask* pBigTask = mBigWorkingTaskList.take(id);
			delete pBigTask; pBigTask = nullptr;
		}
		else
		{
			changedState(44);
			mErrorTaskList.append(info);
		}

		if (mBigWaitTaskList.size())
		{
			BigTask* pBigTask = mBigWaitTaskList.dequeue();
			doBigTask(pBigTask);
		}
		return;
	}
	qDebug() << __FUNCTION__ << "big work task is not existed." << id;
}

BigTask::BigTask(const TaskData& info, QObject* parent)
	: QObject(parent)
	, mBigTask(info)
{

}

BigTask::~BigTask()
{

}

TaskData BigTask::getTaskInfo()
{
	return mBigTask;
}

void BigTask::run()
{
	//download file and make avatar
	bool ret = false;
	do 
	{
		if (!download())
			break;

		if (!makeAvatar())
			break;
		ret = true;
	} while (false);
	emit retBigTaskResult(mBigTask.id, ret, mAvatar);
}

void BigTask::startBigTask()
{
	QThreadPool::globalInstance()->start(this);
}

void BigTask::stopBigTask()
{
	QThreadPool::globalInstance()->cancel(this);
}

bool BigTask::download()
{
	quint64 filesize = 0;
	bool ret = false;
	QNetworkRequest request;
	if (mBigTask.url.startsWith("https://"))
	{
		QSslConfiguration config;
		config.setPeerVerifyMode(QSslSocket::VerifyNone);
		request.setSslConfiguration(config);
	}
	request.setUrl(mBigTask.url);
	QNetworkAccessManager manager;
	QNetworkReply	*pReply = manager.get(request);
	ReplyTimeout *pTimeout = new ReplyTimeout(pReply, 3000);
	connect(this, &BigTask::cancelTask, pReply, &QNetworkReply::abort, Qt::QueuedConnection);

	QEventLoop loop;
	connect(pReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	QNetworkReply::NetworkError errorCode = pReply->error();
	if (errorCode == QNetworkReply::NoError && !pTimeout->isTimeout())
	{
		QByteArray buffer = pReply->readAll();
		filesize = buffer.size();
		if (0 < filesize && smaxsize > filesize)
		{
			ret = mAvatar.loadFromData(buffer);
		}
	}
	emit printLog(QString("%5: id=%1; url=%2; name=%3; result=%4; size=%6; error=%7;")
		.arg(mBigTask.id)
		.arg(mBigTask.url)
		.arg(mBigTask.fileName)
		.arg(QString::number(ret))
		.arg(__FUNCTION__)
		.arg(filesize)
		.arg(pReply->errorString()));
	return ret;
}

bool BigTask::makeAvatar()
{
	mAvatar = MakeRoundAvatar(mAvatar, mBigTask.size);
	return true;
}