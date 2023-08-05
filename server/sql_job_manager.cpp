﻿/*
	Copyright (c) 2017 TOSHIBA Digital Solutions Corporation
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as
	published by the Free Software Foundation, either version 3 of the
	License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Affero General Public License for more details.

	You should have received a copy of the GNU Affero General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "sql_job_manager.h"
#include "sql_execution.h"
#include "sql_compiler.h"
#include "data_store_v4.h"
#include "sql_processor.h"
#include "sql_service.h"
#include "uuid_utils.h"

#include "sql_processor_ddl.h"
#include "sql_processor_result.h"




UTIL_TRACER_DECLARE(SQL_SERVICE);
UTIL_TRACER_DECLARE(DISTRIBUTED_FRAMEWORK);
UTIL_TRACER_DECLARE(DISTRIBUTED_FRAMEWORK_DETAIL);
UTIL_TRACER_DECLARE(SQL_INTERNAL);

#define DUMP_SEND_CONTROL(s) \
	GS_TRACE_INFO(DISTRIBUTED_FRAMEWORK_DETAIL, 0, s);

static const char* getProcessorName(SQLType::Id type) {
	return SQLType::Coder()(type, "UNDEF");
}

static const char* getControlName(JobManager::ControlType type) {
	switch (type) {
	case JobManager::FW_CONTROL_DEPLOY: return "DEPLOY";
	case JobManager::FW_CONTROL_DEPLOY_SUCCESS: return "DEPLOY_SUCCESS";
	case JobManager::FW_CONTROL_PIPE0: return "PIPE0";
	case JobManager::FW_CONTROL_PIPE: return "PIPE";
	case JobManager::FW_CONTROL_FINISH: return "FINISH";
	case JobManager::FW_CONTROL_NEXT: return "NEXT";
	case JobManager::FW_CONTROL_EXECUTE_SUCCESS: return "EXECUTE_SUCCESS";
	case JobManager::FW_CONTROL_CANCEL: return "CANCEL";
	case JobManager::FW_CONTROL_CLOSE: return "CLOSE";
	default: return "UNDEF";
	}
}

static const char* getTaskExecStatusName(
	JobManager::TaskExecutionStatus type) {
	switch (type) {
	case JobManager::TASK_EXEC_PIPE: return "PIPE";
	case JobManager::TASK_EXEC_FINISH: return "FINISH";
	case JobManager::TASK_EXEC_NEXT: return "NEXT";
	case JobManager::TASK_EXEC_IDLE: return "IDLE";
	default: return "UNDEF";
	}
}

#define TRACE_TASK_START(task, inputId, controlType) \
	GS_TRACE_INFO(DISTRIBUTED_FRAMEWORK_DETAIL, \
			GS_TRACE_SQL_INTERNAL_DEBUG, \
			" " << task->job_->jobId_ << " " << \
			" " << task->job_->getJobManager()->getHostName() << " " << \
			" TASK " << task->taskId_ << " START IN " << inputId << " " \
			<< getProcessorName(task->sqlType_) \
			<< " " << getControlName(controlType));

#define TRACE_TASK_END(task, inputId, controlType) \
	GS_TRACE_INFO(DISTRIBUTED_FRAMEWORK_DETAIL, \
			GS_TRACE_SQL_INTERNAL_DEBUG, \
			" " << task->job_->jobId_ << " " << \
			" " << task->job_->getJobManager()->getHostName() << " " << \
			" TASK " << task->taskId_ << " END IN " << inputId << " " \
			<< getProcessorName(task->sqlType_) \
			<< " " << getControlName(controlType));

#define TRACE_ADD_EVENT(str, jobId, taskId, inputId, controlType, pId) \
	GS_TRACE_INFO(DISTRIBUTED_FRAMEWORK_DETAIL, \
			GS_TRACE_SQL_INTERNAL_DEBUG, \
			" " << str << \
			" " << jobId << " " << \
			" " << jobManager_->getHostName() << " " << \
			" TASK " << taskId << " EVENT IN " << inputId << " "  << \
			" " << getControlName(controlType));

#define TRACE_ADD_EVENT2(str, jobId, taskId, inputId, controlType, count) \
	GS_TRACE_INFO(DISTRIBUTED_FRAMEWORK_DETAIL, \
			GS_TRACE_SQL_INTERNAL_DEBUG, \
			" " << str << \
			" " << jobId << " " << \
			" " << jobManager_->getHostName() << " " << \
			" TASK " << taskId << " EVENT IN " << inputId << " "  << \
			" " << getControlName(controlType) << " " << count);


#define TRACE_NEXT_EVENT(task, inputId, controlType) \
	GS_TRACE_INFO(DISTRIBUTED_FRAMEWORK_DETAIL, \
			GS_TRACE_SQL_INTERNAL_DEBUG, \
			"CONT" << " " << task->job_->jobId_ << " " << \
			" " << task->job_->jobManager_->getHostName() << " " << \
			" TASK " << task->taskId_ << " EVENT IN " << inputId << " " \
			<< getProcessorName(task->sqlType_) \
			<< " " << getControlName(controlType));

#define TRACE_TASK_COMPLETE(task, outputId) \
	GS_TRACE_INFO(DISTRIBUTED_FRAMEWORK_DETAIL, \
			GS_TRACE_SQL_INTERNAL_DEBUG, \
			" " << task->job_->getJobId() << " " << \
			" " << task->job_->getJobManager()->getHostName() << " " << \
			" TASK " << task->taskId_ << " COMPLETE OUT " << outputId << " " \
			<< getProcessorName(task->sqlType_));


#define TRACE_JOB_CONTROL_START(jobId, hostName, controlName) \
	GS_TRACE_INFO(DISTRIBUTED_FRAMEWORK_DETAIL, \
			GS_TRACE_SQL_INTERNAL_DEBUG, \
			" " << jobId << " " << \
			" " << hostName << " " << \
			" TASK " << 0 << " START IN " << 0 << " " \
			<< controlName \
			<< " " << "PIPE");

#define TRACE_CONTINUOUS_BLOCK(jobId, taskId, inputId, controlType) \
	GS_TRACE_INFO(DISTRIBUTED_FRAMEWORK_DETAIL, \
			GS_TRACE_SQL_INTERNAL_DEBUG, \
			" " << jobId << " " << \
			" " << jobManager_->getHostName() << " " << \
			" TASK " << taskId << " EMPTY BLOCK IN " << inputId << " " \
			<< getProcessorName(task->sqlType_) \
			<< " " << getControlName(controlType));

#define TRACE_JOB_CONTROL_END(jobId, hostName, controlName) \
	GS_TRACE_INFO(DISTRIBUTED_FRAMEWORK_DETAIL, \
			GS_TRACE_SQL_INTERNAL_DEBUG, \
			" " << jobId << " " << \
			" " << hostName << " " << \
			" TASK " << 0 << " END IN " << 0 << " " \
			<< controlName \
			<< " " << "PIPE");

JobManager::JobManager(SQLVariableSizeGlobalAllocator& globalVarAlloc,
	LocalTempStore& store, const ConfigTable& config) :
	globalVarAlloc_(globalVarAlloc),
	jobMap_(JobMap::key_compare(), globalVarAlloc_),
	store_(store),
	clsSvc_(NULL),
	pt_(NULL),
	txnSvc_(NULL),
	sqlSvc_(NULL),
	dataStore_(NULL),
	executionManager_(NULL),
	currentCnt_(0),
	txnConcurrency_(config.get<int32_t>(CONFIG_TABLE_DS_CONCURRENCY)),
	sqlConcurrency_(config.get<int32_t>(CONFIG_TABLE_SQL_CONCURRENCY)),
	sqlPgConfig_(KeyDataStore::MAX_PARTITION_NUM, sqlConcurrency_),
	txnPgConfig_(config.get<int32_t>(CONFIG_TABLE_DS_PARTITION_NUM), txnConcurrency_),
	sendRequestCount_(0)
{
	try {
		sqlConcurrency_ = config.get<int32_t>(CONFIG_TABLE_SQL_CONCURRENCY);
		sqlStartPIdList_.assign(sqlConcurrency_, 0);
		for (int32_t pgId = 0; pgId < sqlConcurrency_; pgId++) {
			sqlStartPIdList_[pgId] = sqlPgConfig_.getGroupBeginPartitionId(pgId);
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

JobManager::~JobManager() try {
	for (JobMap::iterator it = jobMap_.begin(); it != jobMap_.end(); it++) {
		Job* job = (*it).second;
		if (job != NULL) {
			ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, job);
		}
	}
	jobMap_.clear();
}
catch (...) {
}

void JobManager::initialize(const ManagerSet& mgrSet) {
	if (mgrSet.clsSvc_ && mgrSet.clsSvc_->getEE()) {
		eeList_.push_back(mgrSet.clsSvc_->getEE());
	}
	if (mgrSet.txnSvc_ && mgrSet.txnSvc_->getEE()) {
		eeList_.push_back(mgrSet.txnSvc_->getEE());
	}
	if (mgrSet.syncSvc_ && mgrSet.syncSvc_->getEE()) {
		eeList_.push_back(mgrSet.syncSvc_->getEE());
	}
	if (mgrSet.sysSvc_ && mgrSet.sysSvc_->getEE()) {
		eeList_.push_back(mgrSet.sysSvc_->getEE());
	}
	if (mgrSet.sqlSvc_ && mgrSet.sqlSvc_->getEE()) {
		eeList_.push_back(mgrSet.sqlSvc_->getEE());
	}
	sqlSvc_ = mgrSet.sqlSvc_;
	txnSvc_ = mgrSet.txnSvc_;
	clsSvc_ = mgrSet.clsSvc_;
	pt_ = mgrSet.pt_;
	executionManager_ = mgrSet.execMgr_;
	getHostInfo();
}

JobManager::Latch::Latch(JobId& jobId, const char* str, JobManager* jobManager,
	LatchMode mode, JobInfo* jobInfo) :
	jobId_(jobId), jobManager_(jobManager), job_(NULL), str_(str) {
	if (mode == LATCH_CREATE) {
		job_ = jobManager_->create(jobId, jobInfo);
	}
	else {
		job_ = jobManager_->get(jobId);
	}
}

JobManager::Latch::~Latch() {
	if (job_) jobManager_->release(job_, str_);
}

Job* JobManager::Latch::get() {
	return job_;
}

Job* JobManager::create(JobId& jobId, JobInfo* jobInfo) {
	try {
		util::LockGuard<util::Mutex> guard(mutex_);
		JobMapItr it = jobMap_.find(jobId);
		if (it != jobMap_.end()) {
			GS_THROW_USER_ERROR(GS_ERROR_JOB_ALREADY_EXISTS,
				"JobId=" << jobId << " is already exists");
		}
		else {
			Job* job = ALLOC_VAR_SIZE_NEW(globalVarAlloc_)
				Job(jobId, jobInfo, this, globalVarAlloc_);
			job->incReference();
			jobMap_.insert(std::make_pair(jobId, job));
			return job;
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void JobManager::remove(JobId& jobId) {
	try {
		util::LockGuard<util::Mutex> guard(mutex_);
		JobMapItr it = jobMap_.find(jobId);
		if (it != jobMap_.end()) {
			Job* job = (*it).second;
			if (job->isCoordinator()) {
				job->setCancel();
			}
			if (job->decReference()) {
				ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, job);
			}
			jobMap_.erase(it);
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

Job* JobManager::get(JobId& jobId) {
	try {
		util::LockGuard<util::Mutex> guard(mutex_);
		JobMapItr it = jobMap_.find(jobId);
		if (it != jobMap_.end()) {
			Job* job = (*it).second;
			job->incReference();
			return job;
		}
		else {
			return NULL;
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void JobManager::release(Job* job, const char* str) {
	try {
		util::LockGuard<util::Mutex> guard(mutex_);
		if (job->decReference()) {
			util::StackAllocator* alloc = job->getLocalStackAllocator();
			ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, job);
			executionManager_->releaseStackAllocator(alloc);
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void JobManager::beginJob(EventContext& ec, JobId& jobId,
	JobInfo* jobInfo, int64_t waitInterval) {
	util::StackAllocator& alloc = ec.getAllocator();
	try {
		Latch latch(jobId, "JobManager::beginJob", this, JobManager::Latch::LATCH_CREATE, jobInfo);
		Job* job = latch.get();

		TRACE_JOB_CONTROL_START(jobId, getHostName(), "DEPLOY");
		job->deploy(ec, alloc, jobInfo, 0, waitInterval);
		TRACE_JOB_CONTROL_END(jobId, getHostName(), "DEPLOY");
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e,
			"Execute SQL failed (reason=" << GS_EXCEPTION_MESSAGE(e)
			<< " (jobId=" << jobId << ", operation="
			<< getControlName(FW_CONTROL_DEPLOY) << ")");
	}
}

void JobManager::cancel(EventContext& ec, JobId& jobId, bool clientCancel) {
	try {
		Latch latch(jobId, "JobManager::cancel", this);
		Job* job = latch.get();
		if (job == NULL) {
			GS_TRACE_INFO(SQL_SERVICE,
				GS_TRACE_SQL_EXECUTION_INFO, "CANCEL:jobId=" << jobId);
			return;
		}
		job->cancel(ec, clientCancel);
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void JobManager::cancelAll(const Event::Source& eventSource,
	util::StackAllocator& alloc, util::Vector<JobId>& jobIdList,
	CancelOption& cancelOption) {
	try {
		if (jobIdList.empty()) {
			getCurrentJobList(jobIdList);
		}
		for (size_t pos = 0; pos < jobIdList.size(); pos++) {
			cancel(eventSource, jobIdList[pos], cancelOption);
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void JobManager::checkTimeout(EventContext& ec,
	EventMonotonicTime currentTime) {
	try {
		util::StackAllocator& alloc = ec.getAllocator();
		util::Vector<JobId> jobIdList(alloc);
		getCurrentJobList(jobIdList);
		for (util::Vector<JobId>::iterator it = jobIdList.begin();
			it != jobIdList.end(); it++) {
			checkAlive(ec, alloc, (*it), currentTime);
		}
	}
	catch (std::exception& e) {
		UTIL_TRACE_EXCEPTION(SQL_SERVICE, e, "");
	}
}

CancelOption::CancelOption(util::String& startTimeStr) : startTime_(0), forced_(false) {
	if (startTimeStr.size() > 0) {
		startTime_ = convertToTime(startTimeStr);
	}
}
void JobManager::resetSendCounter(util::StackAllocator& alloc) {

	util::Vector<JobId> jobIdList(alloc);
	getCurrentJobList(jobIdList);

	const char* label = "JobManager::resetSendCounter";

	for (size_t pos = 0; pos < jobIdList.size(); pos++) {
		Latch latch(jobIdList[pos], label, this);
		Job* job = latch.get();
		if (job) {
			job->resetSendCounter();
		}
	}
}

void JobManager::getProfiler(util::StackAllocator& alloc,
	StatJobProfiler& jobProfiler, int32_t mode, JobId* currentJobId) {

	util::Vector<JobId> jobIdList(alloc);
	getCurrentJobList(jobIdList);

	int64_t jobCount = 0;
	for (size_t pos = 0; pos < jobIdList.size(); pos++) {
		Latch latch(jobIdList[pos], "JobManager::getProfiler", this);
		Job* job = latch.get();
		if (job) {
			if (currentJobId) {
				if (*currentJobId == job->getJobId()) {
				}
				else {
					continue;
				}
			}
			StatJobProfilerInfo jobProf(alloc, job->getJobId());
			jobProf.id_ = jobCount++;
			jobProf.startTime_ = CommonUtility::getTimeStr(job->getStartTime()).c_str();
			jobProf.deployCompleteTime_ = CommonUtility::getTimeStr(job->deployCompleteTime_).c_str();
			jobProf.execStartTime_ = CommonUtility::getTimeStr(job->execStartTime_).c_str();
			jobProf.sendEventCount_ = job->sendEventCount_;
			jobProf.sendEventSize_ = job->sendEventSize_;
			jobProf.address_ = getHostName().c_str();

			jobProf.sendPendingCount_ = job->sendRequestCount_;
			job->getProfiler(alloc, jobProf, mode);
			jobProfiler.jobProfs_.push_back(jobProf);
			if (currentJobId) {
				break;
			}
		}
	}
}

void JobManager::getJobIdList(
	util::StackAllocator& alloc, StatJobIdList& infoList) {
	util::LockGuard<util::Mutex> guard(mutex_);
	for (JobMapItr it = jobMap_.begin(); it != jobMap_.end();it++) {
		Job* job = (*it).second;
		if (job) {
			StatJobIdListInfo info(alloc);
			info.startTime_ = CommonUtility::getTimeStr(job->getStartTime()).c_str();
			util::String uuidStr(alloc);
			job->getJobId().toString(alloc, uuidStr);
			info.jobId_ = uuidStr;
			infoList.infoList_.push_back(info);
		}
	}
}

void JobManager::getCurrentJobList(util::Vector<JobId>& jobIdList) {
	util::LockGuard<util::Mutex> guard(mutex_);
	for (JobMapItr it = jobMap_.begin(); it != jobMap_.end();it++) {
		jobIdList.push_back((*it).second->getJobId());
	}
}

size_t JobManager::getCurrentJobListSize() {
	util::LockGuard<util::Mutex> guard(mutex_);
	return jobMap_.size();
}

void JobManager::checkAlive(EventContext& ec,
	util::StackAllocator& alloc, JobId& jobId, EventMonotonicTime checkTime) {
	Job* job = NULL;
	Latch latch(jobId, "JobManager::checkAlive", this);
	job = latch.get();
	if (job == NULL) {
		GS_TRACE_DEBUG(SQL_SERVICE,
			GS_TRACE_SQL_EXECUTION_INFO, "jobId=" << jobId);
		return;
	}
	try {
		RAISE_OPERATION(SQLFailureSimulator::TARGET_POINT_1);
		job->checkCancel("timer check", true, true);
	}
	catch (std::exception& e) {
		UTIL_TRACE_EXCEPTION(SQL_SERVICE, e, "timer check cancel");
		if (job) {
			job->setCancel();
			try {
				job->handleError(ec, &e);
			}
			catch (std::exception& e2) {
				UTIL_TRACE_EXCEPTION(SQL_SERVICE, e2, "");
			}
			if (!job->isCoordinator()) {
				remove(jobId);
			}
		}
	}
}

void JobManager::cancel(
	const Event::Source& source, JobId& jobId, CancelOption& option) {
	try {
		Latch latch(jobId, "JobManager::cancel2", this);
		Job* job = latch.get();
		if (job == NULL) {
			GS_TRACE_DEBUG(SQL_SERVICE,
				GS_TRACE_SQL_EXECUTION_INFO, "CANCEL:jobId=" << jobId);
			return;
		}
		if (option.startTime_ > 0) {
			if (job->getStartTime() < option.startTime_) {
				return;
			}
		}

		job->cancel(source, option);
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

Job::Job(JobId& jobId, JobInfo* jobInfo, JobManager* jobManager,
	SQLVariableSizeGlobalAllocator& globalVarAlloc) :
	jobId_(jobId),
	jobManager_(jobManager),
	jobStackAlloc_(*getStackAllocator()),
	scope_(NULL),
	globalVarAlloc_(globalVarAlloc),
	noCondition_(globalVarAlloc_),
	nodeList_(globalVarAlloc_),
	taskList_(globalVarAlloc_),
	taskStatList_(globalVarAlloc_),
	deployCompletedCount_(0),
	executeCompletedCount_(0),
	taskCompletedCount_(0),
	refCount_(1),
	selfTaskNum_(0),
	deployCompleted_(false),
	coordinatorNodeId_(jobInfo->coordinatorNodeId_),
	cancelled_(false),
	clientCancelled_(false),
	status_(FW_JOB_INIT),
	jobAllocateMemory_(0),
	executionCount_(0),
	coordinator_(jobInfo->coordinator_),
	queryTimeout_(jobInfo->queryTimeout_),
	expiredTime_(jobInfo->expiredTime_),
	startTime_(jobInfo->startTime_),
	startMonotonicTime_(jobInfo->startMonotonicTime_),
	isSetJobTime_(jobInfo->isSetJobTime_),
	resultTaskId_(jobInfo->resultTaskId_),
	connectedPId_(jobInfo->pId_),
	syncContext_(jobInfo->syncContext_),
	baseTaskId_(UNDEF_TASKID),
	limitedAssignNoList_(globalVarAlloc_),
	limitAssignNum_(jobInfo->limitAssignNum_),
	currentAssignNo_(0),
	profilerInfos_(jobStackAlloc_),
	isExplainAnalyze_(jobInfo->isExplainAnalyze_),
	dispatchNodeIdList_(globalVarAlloc_),
	dispatchServiceList_(globalVarAlloc_),
	sendRequestCount_(0),
	limitPendingTime_(0),
	sendVersion_(0),
	point_(0),
	sendJobCount_(0),
	sendEventCount_(0),
	deployCompleteTime_(0),
	execStartTime_(0),
	str_(NULL),
	isSQL_(jobInfo->isSQL_),
	storeMemoryAgingSwapRate_(jobInfo->storeMemoryAgingSwapRate_),
	timezone_(jobInfo->timezone_)
{

	setLimitedAssignNoList();
	GsNodeInfo* nodeInfo = NULL;
	NodeId nodeId;
	for (size_t pos = 0; pos < jobInfo->gsNodeInfoList_.size(); pos++) {
		nodeInfo = jobInfo->gsNodeInfoList_[pos];
		if (coordinator_) {
			nodeId = nodeInfo->nodeId_;
		}
		else {
			nodeId = resolveNodeId(nodeInfo);
		}
		nodeList_.push_back(nodeId);
	}
	scope_ = ALLOC_VAR_SIZE_NEW(globalVarAlloc_)
		util::StackAllocator::Scope(jobStackAlloc_);
}

Job::~Job()
{
	for (size_t pos = 0;pos < taskList_.size(); pos++) {
		Task* task = taskList_[pos];
		if (task != NULL) {
			ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, task);
			task = NULL;
		}
	}
	for (size_t pos = 0; pos < noCondition_.size(); pos++) {
		ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, noCondition_[pos]);
	}
	noCondition_.clear();

	util::Vector<TaskProfilerInfo*>& profilerList = profilerInfos_.profilerList_;
	for (size_t pos = 0; pos < profilerList.size(); pos++) {
		ALLOC_DELETE(jobStackAlloc_, profilerList[pos]);
	}
	ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, scope_);
}

Task* Job::createTask(EventContext& ec, TaskId taskId,
	JobInfo* jobInfo, TaskInfo* taskInfo) {
	Task* task = NULL;
	try {
		task = ALLOC_VAR_SIZE_NEW(globalVarAlloc_) Task(this, taskInfo);
		if (!isDQLProcessor(taskInfo->sqlType_)) {
			task->cxt_->setEventContext(&ec);
		}
		if (task->isDml_) {
			if (jobInfo->containerType_ == TIME_SERIES_CONTAINER) {
				task->cxt_->setDMLTimeSeries();
			}
		}
		if (taskInfo->sqlType_ == SQLType::EXEC_DDL) {
			baseTaskId_ = task->taskId_;
		}
		task->createProcessor(ec.getAllocator(), jobInfo, taskId, taskInfo);
		if (task->isImmediate()) {
			noCondition_.push_back(
				ALLOC_VAR_SIZE_NEW(globalVarAlloc_) Dispatch(task->taskId_, task->nodeId_,
					0, task->getServiceType(), task->loadBalance_));
		}
		selfTaskNum_++;
		return task;
	}
	catch (std::exception& e) {
		ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, task);
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

JobManager::Job::TaskInterruptionHandler::TaskInterruptionHandler() :
	cxt_(NULL),
	intervalMillis_(JobManager::DEFAULT_TASK_INTERRUPTION_INTERVAL) {
}

JobManager::Job::TaskInterruptionHandler::~TaskInterruptionHandler() {
}

bool JobManager::Job::TaskInterruptionHandler::operator()(
	InterruptionChecker& checker, int32_t flags) {
	EventContext* ec = cxt_->getEventContext();
	Task* task = cxt_->getTask();
	Job* job = task->job_;
	assert(ec != NULL && task != NULL && job != NULL);
	job->checkCancel("processor", false);
	return ec->getEngine().getMonotonicTime() -
		ec->getHandlerStartMonotonicTime() > intervalMillis_;
}

JobManager::Job::Task::Task(Job* job, TaskInfo* taskInfo) :
	job_(job),
	jobManager_(job->jobManager_),
	jobStackAlloc_(job->jobStackAlloc_),
	processorStackAlloc_(NULL),
	processorVarAlloc_(NULL),
	globalVarAlloc_(job->globalVarAlloc_),
	outputDispatchList_(taskInfo->outputList_.size(),
		DispatchList(globalVarAlloc_), globalVarAlloc_),
	completed_(false),
	resultComplete_(false),
	immediate_(false),
	counter_(0),
	dispatchTime_(0),
	dispatchCount_(0),
	status_(JobManager::TASK_EXEC_IDLE),
	sendBlockCounter_(0),
	tempTupleList_(globalVarAlloc_),
	blockReaderList_(globalVarAlloc_),
	recvBlockCounterList_(globalVarAlloc_),
	inputStatusList_(globalVarAlloc_),
	inputNo_(0),
	startTime_(0),
	taskId_(taskInfo->taskId_),
	nodeId_(taskInfo->nodeId_),
	cxt_(NULL),
	type_(taskInfo->type_),
	sqlType_(taskInfo->sqlType_),
	loadBalance_(taskInfo->loadBalance_),
	isDml_(taskInfo->isDml_),
	processor_(NULL),
	group_(jobManager_->getStore()),
	processorGroupId_(LocalTempStore::UNDEF_GROUP_ID),
	setGroup_(false),
	profilerInfo_(NULL),
	scope_(NULL),
	sendEventCount_(0),
	sendEventSize_(0),
	sendRequestCount_(0)
{
	if (taskInfo->inputList_.size() == 0) {
		immediate_ = true;
	}
	inputStatusList_.assign(taskInfo->inputList_.size(), 0);
	for (size_t pos = 0; pos < taskInfo->inputTypeList_.size(); pos++) {
		TupleList::Info info;
		info.columnCount_ = taskInfo->inputTypeList_[pos].size();
		info.columnTypeList_ = &taskInfo->inputTypeList_[pos][0];
		TupleList* tupleList =
			ALLOC_VAR_SIZE_NEW(globalVarAlloc_) TupleList(group_, info);
		tempTupleList_.push_back(tupleList);
		TupleList::BlockReader* reader
			= ALLOC_VAR_SIZE_NEW(globalVarAlloc_) TupleList::BlockReader(*tupleList);
		reader->setOnce();
		blockReaderList_.push_back(reader);
	}
	recvBlockCounterList_.assign(taskInfo->inputList_.size(), -1);
	if (loadBalance_ == -1) {
		loadBalance_ = job_->getAssignPId();
	}
	taskInfo->loadBalance_ = loadBalance_;
	processorStackAlloc_ = job_->getStackAllocator();
	processorVarAlloc_ = job_->getLocalVarAllocator();
	scope_ = ALLOC_VAR_SIZE_NEW(globalVarAlloc_)
		util::StackAllocator::Scope(*processorStackAlloc_);
	cxt_ = ALLOC_VAR_SIZE_NEW(globalVarAlloc_) SQLContext(
		&globalVarAlloc_, NULL,
		processorStackAlloc_, processorVarAlloc_, jobManager_->getStore(),
		jobManager_->getProcessorConfig());

	setupContextBase(cxt_, jobManager_);
	setupContextTask();
	setupProfiler(taskInfo);

	interruptionHandler_.cxt_ = cxt_;
	cxt_->setInterruptionHandler(&interruptionHandler_);
}

JobManager::Job::Task::~Task() try
{
	SQLProcessor::Factory factory;
	factory.destroy(processor_);
	for (size_t i = 0; i < blockReaderList_.size(); i++) {
		ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, blockReaderList_[i]);
	}
	for (size_t i = 0; i < tempTupleList_.size(); i++) {
		ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, tempTupleList_[i]);
	}
	tempTupleList_.clear();
	blockReaderList_.clear();

	for (size_t i = 0; i < outputDispatchList_.size(); i++) {
		for (size_t j = 0; j < outputDispatchList_[i].size(); j++) {
			ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, outputDispatchList_[i][j]);
		}
	}

	outputDispatchList_.clear();
	ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, cxt_);
	ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, scope_);

	job_->releaseStackAllocator(processorStackAlloc_);
	job_->releaseLocalVarAllocator(processorVarAlloc_);

}
catch (...) {
	assert(false);
}

bool Job::checkStart() {
	int32_t currentCount = ++deployCompletedCount_;
	if (currentCount == static_cast<int32_t>(nodeList_.size())) {
		return true;
	}
	else {
		return false;
	}
}

void Job::encodeProcessor(EventContext& ec,
	util::StackAllocator& alloc, JobInfo* jobInfo) {
	util::XArrayOutStream<>& out = jobInfo->outStream_;
	util::ByteStream< util::XArrayOutStream<> > currentOut(out);
	SQLProcessor::Factory factory;
	SQLVarSizeAllocator varAlloc(util::AllocatorInfo(ALLOCATOR_GROUP_SQL_WORK,
		"TaskVarAllocator"));
	if (jobInfo->taskInfoList_.size() != taskList_.size()) {
		GS_THROW_USER_ERROR(GS_ERROR_JOB_INTERNAL,
			"Job internal error, invalid jobInfo, expect="
			<< jobInfo->taskInfoList_.size() << ", actual=" << taskList_.size());
	}
	for (size_t pos = 0; pos < jobInfo->taskInfoList_.size(); pos++) {
		assert(jobInfo->taskInfoList_[pos]);
		if (!isDQLProcessor(jobInfo->taskInfoList_[pos]->sqlType_)) {
			continue;
		}
		const size_t startPos = currentOut.base().position();
		currentOut << static_cast<uint32_t>(0);
		Task* task = taskList_[pos];
		if (task != NULL) {
			assert(task->processor_);
			SQLProcessor::makeCoder(util::ObjectCoder(),
				*task->cxt_, NULL, NULL).encode(currentOut, task->processor_);
		}
		else {
			TaskOption& option = jobInfo->option_;
			SQLContext cxt(&globalVarAlloc_, NULL, &alloc,
				&varAlloc, jobManager_->getStore(),
				jobManager_->getProcessorConfig());
			Task::setupContextBase(&cxt, jobManager_);
			option.planNodeId_ = static_cast<uint32_t>(pos);
			SQLProcessor* processor = factory.create(
				cxt, option, jobInfo->taskInfoList_[pos]->inputTypeList_);
			SQLProcessor::makeCoder(util::ObjectCoder(),
				cxt, NULL, NULL).encode(currentOut, processor);
			factory.destroy(processor);
		}
		const size_t endPos = currentOut.base().position();
		uint32_t bodySize = static_cast<uint32_t>(endPos - startPos - sizeof(uint32_t));
		currentOut.base().position(startPos);
		currentOut << static_cast<uint32_t>(bodySize);
		currentOut.base().position(endPos);
	}
	encodeOptional(currentOut, jobInfo);
	if (!jobInfo->timezone_.isEmpty()) {
		currentOut << jobInfo->timezone_.getOffsetMillis();
	}
}


void Job::checkCancel(const char* str, bool partitionCheck, bool nodeCheck) {

	SQLService* sqlService = jobManager_->getSQLService();

	if (expiredTime_ != INT64_MAX) {
		int64_t current = sqlService->getEE()->getMonotonicTime();
		if (current >= expiredTime_) {
			GS_THROW_USER_ERROR(GS_ERROR_JOB_CANCELLED,
				"Canceling job due to query timeout "
					"(jobId=" << jobId_ << ", query timeout=" << queryTimeout_ << " (ms), "
					"query start time=" << CommonUtility::getTimeStr(startTime_) << ")");
		}
	}

	try {
		jobManager_->getSQLService()->checkActiveStatus();
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_ERROR_CODED(GS_ERROR_JOB_CANCELLED, e,
			GS_EXCEPTION_MERGE_MESSAGE(e, 
				"Canceling job due to invalid cluster status "
					"(jobId=" << jobId_ << ")"));
	}

	if (nodeCheck) {
		PartitionTable* pt = jobManager_->getPartitionTable();
		for (size_t pos = 0; pos < nodeList_.size(); pos++) {
			if (pt->getHeartbeatTimeout(nodeList_[pos]) == UNDEF_TTL) {
				GS_THROW_USER_ERROR(GS_ERROR_JOB_CANCELLED,
					"Canceling job due to node failures, "
					"(jobId=" << jobId_ << ", failured node=" << pt->dumpNodeAddress(nodeList_[pos]) << ")");
			}
		}
	}

	if (partitionCheck) {
		try {
			checkAsyncPartitionStatus();
		}
		catch (std::exception& e) {
			GS_RETHROW_USER_ERROR_CODED(GS_ERROR_JOB_CANCELLED, e, \
				GS_EXCEPTION_MERGE_MESSAGE(e, "Canceling job due to invalid partition status ("
				"jobId=" << jobId_ << ")"));
		}
	}
	
	if (cancelled_) {
		if (clientCancelled_) {
			SQLExecutionManager::Latch latch(jobId_.clientId_,
				jobManager_->getExecutionManager());
			SQLExecution* execution = latch.get();
			if (execution) {
				execution->getContext().getClientNd();
				GS_THROW_USER_ERROR(GS_ERROR_JOB_CANCELLED,
					"Canceling job due to client cancel request or query timeout ("
					"jobId=" << jobId_ <<
					 "client address=" << execution->getContext().getClientNd() << 
					 ", db=" << execution->getContext().getDBName() <<
					 ", app=" << execution->getContext().getApplicationName() <<
					 ", source=" << execution->getContext().getClientNd() << ")");
			}
		}
		int64_t memoryLimit = jobManager_->sqlSvc_->getJobMemoryLimit();
		if (memoryLimit > 0 && jobAllocateMemory_ >= memoryLimit) {
			GS_THROW_USER_ERROR(GS_ERROR_JOB_CANCELLED,
				"Canceling job due to memory allocation limit ("
				"jobId=" << jobId_ << ", allocation size=" << jobAllocateMemory_ <<
				", limit size = " << memoryLimit << ")");
		}
		GS_THROW_USER_ERROR(GS_ERROR_JOB_CANCELLED,
			"Canceling job due to internal process (jobId=" << jobId_ << ")");
	}
}

void JobManager::Job::cancel(EventContext& ec, bool clientCancel) {
	setCancel(clientCancel);
	if (isCoordinator()) {
		for (size_t pos = 0; pos < nodeList_.size(); pos++) {
			NodeId targetNodeId = nodeList_[pos];
			if (pos == 0 && !clientCancel) {
				jobManager_->remove(jobId_);
			}
			else {
				sendJobEvent(ec, targetNodeId, FW_CONTROL_CANCEL, NULL, NULL,
					-1, 0, connectedPId_, SQL_SERVICE, false, false,
					static_cast<TupleList::Block*>(NULL));
			}
		}
	}
	else {
		try {
			RAISE_OPERATION(SQLFailureSimulator::TARGET_POINT_3);
			if (clientCancel) {
				checkCancel("client", false);
			}
			else {
				checkCancel("internal", false);
			}
		}
		catch (std::exception& e) {
			handleError(ec, &e);
			jobManager_->remove(jobId_);
		}
	}
}

void Job::handleError(EventContext& ec, std::exception* e) {
	try {
		const util::Exception checkException = GS_EXCEPTION_CONVERT(*e, "");
		int32_t errorCode = checkException.getErrorCode();
		if (errorCode == GS_ERROR_JOB_CANCELLED) {
			UTIL_TRACE_EXCEPTION_INFO(SQL_SERVICE, *e, "");
		}
		else {
			UTIL_TRACE_EXCEPTION_WARNING(SQL_SERVICE, *e, "");
		}
		int32_t sendNode = 0;
		if (!isCoordinator()) {
			sendNode = coordinatorNodeId_;
		}
		sendJobEvent(ec, sendNode, FW_CONTROL_ERROR, NULL, e,
			-1, 0, connectedPId_, SQL_SERVICE, false, false,
			static_cast<TupleList::Block*>(NULL));
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void JobManager::Job::fetch(EventContext& ec, SQLExecution* execution) {
	RAISE_OPERATION(SQLFailureSimulator::TARGET_POINT_4);
	checkCancel("fetch", false);
	util::StackAllocator& alloc = ec.getAllocator();

	SQLFetchContext fetchContext;
	if (!getFetchContext(fetchContext)) {
		return;
	}
	if (execution->fetch(ec, fetchContext,
		execution->getContext().getExecId(), jobId_.versionId_)) {

		jobManager_->remove(jobId_);
		if (!execution->getContext().isPreparedStatement()) {
			jobManager_->removeExecution(&ec, jobId_, execution, true);
		}
	}
	else if (isCancel()) {
		try {
			GS_THROW_USER_ERROR(GS_ERROR_JOB_CANCELLED, "");
		}
		catch (std::exception& e) {
			jobManager_->executeStatementError(ec, &e, jobId_);
		}
	}
}

PartitionId JobManager::Job::getDispatchId(TaskId taskId, ServiceType& serviceType) {
	util::LockGuard<util::Mutex> guard(mutex_);
	Task* task = taskList_[taskId];
	if (task == NULL) {
		return 0;
	}
	if (task->sqlType_ == SQLType::EXEC_SCAN) {
		serviceType = TRANSACTION_SERVICE;
	}
	else {
		serviceType = SQL_SERVICE;
	}
	return task->loadBalance_;
}

bool JobManager::Job::processNextEvent(
	Task* task, TupleList::Block* block, int64_t blockNo,
	int64_t inputId, ControlType controlType,
	bool isEmpty, bool isLocal) {

	bool isAvailable = true;
	bool isNone = true;

	bool isCompleted = (controlType == JobManager::FW_CONTROL_FINISH);
	bool isExistsBlock = false;
	bool beforeAvailable = false;
	task->blockReaderList_[inputId]->isExists(isExistsBlock, beforeAvailable);

	if (block && (!isEmpty && !isCompleted)) {
		task->tempTupleList_[inputId]->append(*block);
		if (!isLocal) {
			if ((task->recvBlockCounterList_[inputId] + 1) != blockNo) {
				GS_THROW_USER_ERROR(GS_ERROR_JOB_INVALID_RECV_BLOCK_NO,
					"Received block number is not a continuous, expected="
					<< (task->recvBlockCounterList_[inputId] + 1)
					<< ", current=" << blockNo);
			}
			task->recvBlockCounterList_[inputId] ++;
		}
		bool afterExistsBlock = false;
		bool afterAvailable = false;
		task->blockReaderList_[inputId]->isExists(afterExistsBlock, afterAvailable);
		if (!isExistsBlock || (!beforeAvailable && afterAvailable)) {
		}
		else {
			return false;
		}
	}
	else {
		if (controlType == JobManager::FW_CONTROL_FINISH) {
			if (!isExistsBlock) {
				task->inputStatusList_[inputId] = TASK_INPUT_STATUS_FINISHED;
			}
			else {
				task->inputStatusList_[inputId] = TASK_INPUT_STATUS_PREPARE_FINISH;
				return false;
			}
		}
		else {
		}
	}
	return true;
}

void JobManager::Job::receiveTaskBlock(EventContext& ec,
	TaskId taskId, InputId inputId, TupleList::Block* block,
	int64_t blockNo, ControlType controlType,
	bool isEmpty, bool isLocal) {

	util::LockGuard<util::Mutex> guard(mutex_);
	Task* task = taskList_[taskId];
	if (task == NULL) {
		return;
	}
	task->dispatchCount_++;

	if (!processNextEvent(task, block, blockNo, inputId,
		controlType, isEmpty, isLocal)) {
		return;
	}

	PartitionId loadBalance = task->loadBalance_;
	ServiceType serviceType = task->getServiceType();

	TRACE_ADD_EVENT("receive", jobId_, taskId, inputId, controlType, loadBalance);
	Event request(ec, EXECUTE_JOB, loadBalance);
	EventByteOutStream out = request.getOutStream();
	SQLJobHandler::encodeRequestInfo(out, jobId_, controlType);
	bool isCompleted = (controlType == JobManager::FW_CONTROL_FINISH);
	SQLJobHandler::encodeTaskInfo(out, taskId, inputId, isEmpty, isCompleted);
	jobManager_->addEvent(request, serviceType);
}

bool JobManager::Job::getTaskBlock(
	TaskId taskId, InputId inputId, Task*& task,
	TupleList::Block& block, bool& isExistBlock) {
	task = NULL;
	util::LockGuard<util::Mutex> guard(mutex_);
	if (taskList_[taskId] == NULL) {
		return false;
	}
	task = taskList_[taskId];
	isExistBlock = task->blockReaderList_[inputId]->isExists();
	return task->getBlock(inputId, block);
}

void Job::checkMemoryLimit() {

	int64_t memoryLimit = jobManager_->sqlSvc_->getJobMemoryLimit();
	if (memoryLimit == 0) {
		return;
	}

	if (jobAllocateMemory_ >= memoryLimit) {
		setCancel();
	}
}

int64_t Job::getVarTotalSize() {

	if (jobManager_->sqlSvc_->getJobMemoryLimit() == 0) {
		return 0;
	}

	int64_t totalSize = 0;
	for (size_t pos = 0; pos < taskStatList_.size(); pos++) {
		totalSize += taskStatList_[pos];
	}
	return totalSize;
}

int64_t Job::getVarTotalSize(TaskId taskId) {

	if (jobManager_->sqlSvc_->getJobMemoryLimit() == 0) {
		return 0;
	}

	int32_t checkCounter = jobManager_->sqlSvc_->getMemoryCheckCount();
	if (checkCounter == 0) {
		return 0;
	}

	util::LockGuard<util::Mutex> guard(mutex_);
	if (taskList_[taskId] == NULL) {
		return 0;
	}
	Task* task = taskList_[taskId];
	if (task->counter_ % checkCounter == 0) {
	}
	else {
		return 0;
	}

	const size_t count =
		SQLVarSizeAllocator::TraitsType::FIXED_ALLOCATOR_COUNT;
	util::AllocatorStats stats;
	for (size_t i = 0; i < count; i++) {
		task->processorVarAlloc_->base(i)->getStats(stats);
	}
	task->processorVarAlloc_->getStats(stats);
	taskStatList_[taskId] = stats.values_[util::AllocatorStats::STAT_TOTAL_SIZE];
	return taskStatList_[taskId];
}

int64_t Job::getDeployVarTotalSize(TaskId taskId) {

	if (jobManager_->sqlSvc_->getJobMemoryLimit() == 0) {
		return 0;
	}

	int32_t checkCounter = jobManager_->sqlSvc_->getMemoryCheckCount();
	if (checkCounter == 0) {
		return 0;
	}

	if (taskList_[taskId] == NULL) {
		return 0;
	}
	Task* task = taskList_[taskId];
	if (task->counter_ % checkCounter == 0) {
	}
	else {
		return 0;
	}

	const size_t count =
		SQLVarSizeAllocator::TraitsType::FIXED_ALLOCATOR_COUNT;
	util::AllocatorStats stats;
	for (size_t i = 0; i < count; i++) {
		task->processorVarAlloc_->base(i)->getStats(stats);
	}
	task->processorVarAlloc_->getStats(stats);
	taskStatList_[taskId] = stats.values_[util::AllocatorStats::STAT_TOTAL_SIZE];
	return taskStatList_[taskId];
}

void Job::getProfiler(util::StackAllocator& alloc,
	StatJobProfilerInfo& jobProf, int32_t mode) {
	util::LockGuard<util::Mutex> guard(mutex_);

	LocalTempStore& store = jobManager_->getStore();
	util::Mutex& statsMutex = store.getGroupStatsMapMutex();

	int64_t taskVarTotalSize = 0;
	int64_t jobVarTotalSize = 0;

	for (size_t pos = 0; pos < taskList_.size(); pos++) {
		Task* task = taskList_[pos];
		if (taskList_[pos] != NULL) {
			const size_t count =
				SQLVarSizeAllocator::TraitsType::FIXED_ALLOCATOR_COUNT;
			util::AllocatorStats stats;
			for (size_t i = 0; i < count; i++) {
				task->processorVarAlloc_->base(i)->getStats(stats);
			}
			task->processorVarAlloc_->getStats(stats);
			taskVarTotalSize = stats.values_[util::AllocatorStats::STAT_TOTAL_SIZE];
			jobVarTotalSize += taskVarTotalSize;
			task->processorStackAlloc_->getStats(stats);

			if (mode == 1 || (mode != 1 && task->status_ != TASK_EXEC_IDLE)) {
				StatTaskProfilerInfo taskProf(alloc);
				taskProf.id_ = task->taskId_;
				taskProf.inputId_ = task->inputNo_;
				taskProf.sendPendingCount_ = task->sendRequestCount_;
				taskProf.counter_ = task->profilerInfo_->profiler_.executionCount_;
				taskProf.dispatchCount_ = task->dispatchCount_;
				taskProf.dispatchTime_ = task->dispatchTime_;
				taskProf.status_ = getTaskExecStatusName(task->status_);
				taskProf.name_ = getProcessorName(task->sqlType_);
				taskProf.startTime_ = CommonUtility::getTimeStr(task->startTime_).c_str();
				taskProf.sendEventCount_ = task->sendEventCount_;
				taskProf.sendEventSize_ = task->sendEventSize_;
				taskProf.allocateMemory_ = taskVarTotalSize;
				taskProf.worker_ = task->profilerInfo_->profiler_.worker_;
				taskProf.actualTime_ = (task->profilerInfo_->profiler_.actualTime_ / 1000 / 1000);

				util::LockGuard<util::Mutex> guard(statsMutex);
				LocalTempStore::GroupStatsMap& groupStatsMap = store.getGroupStatsMap();

				LocalTempStore::GroupStatsMap::iterator it = groupStatsMap.find(task->group_.getId());
				if (it != groupStatsMap.end()) {
					taskProf.inputSwapRead_ = it->second.swapInCount_;
					taskProf.inputSwapWrite_ = it->second.swapOutCount_;
					taskProf.inputActiveBlockCount_ = it->second.activeBlockCount_;
				}
				if (task->setGroup_) {
					LocalTempStore::GroupStatsMap::iterator it = groupStatsMap.find(task->processorGroupId_);
					if (it != groupStatsMap.end()) {
						taskProf.swapRead_ = it->second.swapInCount_;
						taskProf.swapWrite_ = it->second.swapOutCount_;
						taskProf.activeBlockCount_ = it->second.activeBlockCount_;
					}
				}

				jobProf.taskProfs_.push_back(taskProf);
			}
		}
	}
	jobProf.allocateMemory_ = jobVarTotalSize;
}

void JobManager::Job::checkAsyncPartitionStatus() {
	if (isDDL()) {
		util::LockGuard<util::Mutex> guard(mutex_);
		if (taskList_[baseTaskId_]) {
			checkDDLPartitionStatus();
		}
	}
}

void Job::getSummary(std::string& str) {
	util::LockGuard<util::Mutex> guard(mutex_);
	if (resultTaskId_ < 0 || taskList_.size() <= resultTaskId_) {
		return;
	}
	Task* resultTask = taskList_[resultTaskId_];
	if (resultTask == NULL) {
		return;
	}
	ResultProcessor* resultProcessor =
		static_cast<ResultProcessor*>(resultTask->processor_);
	if (resultProcessor) {
		str = resultProcessor->getProfs().dump();
	}
}

void Job::removeTask(TaskId taskId) {
	util::LockGuard<util::Mutex> guard(mutex_);

	Task* task = taskList_[taskId];

	taskStatList_[taskId] = 0;

	ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, taskList_[taskId]);
	taskList_[taskId] = NULL;
}

void Job::deploy(EventContext& ec, util::StackAllocator& alloc,
	JobInfo* jobInfo, NodeId senderNodeId, int64_t waitInterval) {

	status_ = Job::FW_JOB_DEPLOY;
	util::LockGuard<util::Mutex> guard(mutex_);
	NodeId targetNodeId;
	Task* task = NULL;
	int32_t scanCounter = 0;
	util::Set<NodeId> nodeSet(alloc);

	if (senderNodeId != SELF_NODEID) {
		taskCompletedCount_++;
	}

	for (size_t pos = 0; pos < jobInfo->taskInfoList_.size(); pos++) {
		TaskInfo* taskInfo = jobInfo->taskInfoList_[pos];
		if (taskInfo->nodePos_ == UNDEF_NODEID
			|| taskInfo->nodePos_ >= nodeList_.size()) {
			GS_THROW_USER_ERROR(GS_ERROR_JOB_RESOLVE_NODE_FAILED,
				"Resolve node failed, jobId = " << jobId_ << ", taskPos = " << pos);
		}
		taskInfo->nodeId_ = nodeList_[taskInfo->nodePos_];
		if (taskInfo->sqlType_ == SQLType::EXEC_SCAN && taskInfo->inputList_.size() == 0) {
			nodeSet.insert(taskInfo->nodeId_);
			if (taskInfo->nodeId_ == 0) {
				scanCounter++;
			}
		}
	}

	taskStatList_.assign(jobInfo->taskInfoList_.size(), 0);
	for (size_t pos = 0; pos < jobInfo->taskInfoList_.size(); pos++) {
		TaskInfo* taskInfo = jobInfo->taskInfoList_[pos];
		targetNodeId = nodeList_[taskInfo->nodePos_];
		if ((pos & UNIT_CHECK_COUNT) == 0) {
			RAISE_OPERATION(SQLFailureSimulator::TARGET_POINT_5);
			checkCancel("deploy", false);
		}
		if (coordinator_ && isExplainAnalyze_) {
			dispatchNodeIdList_.push_back(targetNodeId);
			dispatchServiceList_.push_back(taskInfo->type_);
		}
		task = NULL;
		if (targetNodeId == SELF_NODEID
			|| taskInfo->sqlType_ == SQLType::EXEC_RESULT) {
			try {
				task = createTask(ec, static_cast<TaskId>(pos), jobInfo, taskInfo);
			}
			catch (std::exception& e) {
				GS_RETHROW_USER_OR_SYSTEM(e, "");
			}
			taskList_.push_back(task);
			getDeployVarTotalSize(task->taskId_);
		}
		else {
			taskList_.push_back(static_cast<Task*>(NULL));
			if (coordinator_ && isExplainAnalyze_) {
				appendProfiler();
			}
			if (!coordinator_) {
				if (isDQLProcessor(jobInfo->taskInfoList_[pos]->sqlType_)) {
					uint32_t size = 0;
					(*jobInfo->inStream_) >> size;
					const size_t startPos = (*jobInfo->inStream_).base().position();
					(*jobInfo->inStream_).base().position(startPos + size);
				}
			}
		}
	}
	checkCounter_.initLocal(scanCounter);
	if (isCoordinator() && nodeSet.size() >= 1) {
		checkCounter_.initGlobal(nodeSet.size());
	}
	int64_t totalSize = getVarTotalSize();

	if (selfTaskNum_ > 0) {
		int32_t outputId = 0;
		size_t taskSize = taskList_.size();
		for (size_t pos = 0; pos < jobInfo->taskInfoList_.size(); pos++) {
			TaskInfo* taskInfo = jobInfo->taskInfoList_[pos];
			for (InputId inputId = 0; inputId <
				static_cast<InputId>(taskInfo->inputList_.size()); inputId++) {
				TaskId inputTaskId = taskInfo->inputList_[inputId];
				if (taskSize <= inputTaskId) {
					GS_THROW_USER_ERROR(GS_ERROR_SQL_INTERNAL, "");
				}
				Task* inputTask = taskList_[inputTaskId];
				if (inputTask == NULL) {
					continue;
				}
				if (inputTask->outputDispatchList_.size() <= outputId) {
					GS_THROW_USER_ERROR(GS_ERROR_SQL_INTERNAL, "");
				}
				inputTask->outputDispatchList_[outputId].push_back(
					ALLOC_VAR_SIZE_NEW(globalVarAlloc_)
					Dispatch(taskInfo->taskId_, taskInfo->nodeId_,
						inputId, (ServiceType)taskInfo->type_, taskInfo->loadBalance_));
			}
		}
	}
	if (isCoordinator()) {
		recvDeploySuccess(ec, waitInterval);
		sendJobInfo(ec, jobInfo, waitInterval);
	}
	else {
		decodeOptional(*jobInfo->inStream_);
		sendJobEvent(ec, coordinatorNodeId_,
			JobManager::FW_CONTROL_DEPLOY_SUCCESS,
			NULL, NULL, -1, 0, 0, SQL_SERVICE,
			false, false, (TupleList::Block*)NULL, waitInterval);
		if (selfTaskNum_ == 1) {
			sendJobEvent(ec, coordinatorNodeId_,
				JobManager::FW_CONTROL_EXECUTE_SUCCESS,
				NULL, NULL, 0, 0, connectedPId_,
				SQL_SERVICE, false, false, static_cast<TupleList::Block*>(NULL));
			jobManager_->remove(jobId_);
		}
	}
	deployCompleted_ = true;
	deployCompleteTime_ = util::DateTime::now(false).getUnixTime();
}

void Job::request(EventContext* ec, Event* ev, JobManager::ControlType controlType,
	TaskId taskId, TaskId inputId, bool isEmpty, bool isComplete,
	TupleList::Block* block, EventMonotonicTime emTime, int64_t waitTime) {
	const char* taskName = NULL;
	SQLService* sqlSvc = jobManager_->getSQLService();
	try {
		RAISE_OPERATION(SQLFailureSimulator::TARGET_POINT_6);
		checkCancel("request", false);
		switch (controlType) {
		case FW_CONTROL_PIPE0: {
			RAISE_OPERATION(SQLFailureSimulator::TARGET_POINT_11);
			executeJobStart(ec, waitTime);
			break;
		}
		case FW_CONTROL_PIPE:
		case FW_CONTROL_PIPE_FINISH:
		case FW_CONTROL_FINISH:
		case FW_CONTROL_NEXT: {
			RAISE_OPERATION(SQLFailureSimulator::TARGET_POINT_12);
			executeTask(ec, ev, controlType, taskId, inputId,
				isEmpty, isComplete, block, emTime, waitTime);
			break;
		}
		}
		status_ = Job::FW_JOB_EXECUTE;
		if (execStartTime_ == 0) {
			execStartTime_ = util::DateTime::now(false).getUnixTime();
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void Job::executeJobStart(EventContext* ec, int64_t waitTime) {
	try {
		for (size_t pos = 0; pos < noCondition_.size(); pos++) {
			sendJobEvent(*ec, 0, FW_CONTROL_PIPE,
				NULL, NULL, noCondition_[pos]->taskId_,
				0, noCondition_[pos]->loadBalance_,
				static_cast<ServiceType>(noCondition_[pos]->type_),
				true, false, static_cast<TupleList::Block*>(NULL), waitTime);
		}
		status_ = Job::FW_JOB_EXECUTE;
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e,
			"Execute SQL failed (reason=" << GS_EXCEPTION_MESSAGE(e)
			<< " (jobId=" << jobId_ << ", operation="
			<< getControlName(FW_CONTROL_DEPLOY) << ")");
	}
}

void ExecuteJobHandler::executeRequest(EventContext& ec, Event& ev,
	EventByteInStream& in, JobId& jobId, JobManager::ControlType controlType) {
	TaskId taskId = 0;
	InputId inputId = 0;
	PartitionId recvPartitionId = UNDEF_PARTITIONID;
	try {
		bool isEmpty;
		bool isComplete;
		int64_t blockNo = -1;
		decodeTaskInfo(in, taskId, inputId, isEmpty, isComplete, blockNo);
		EventMonotonicTime emTime = ec.getEngine().getMonotonicTime();
		JobManager::Latch latch(jobId, "ExecuteJobHandler::executeRequest", jobManager_);
		Job* job = latch.get();
		if (job == NULL) {
			return;
		}

		if (job->checkTaskInterrupt(taskId)) {

			int64_t waitTime = sqlSvc_->getSendPendingInterval();
			DUMP_SEND_CONTROL(
				"Retry event(pending count="
				<< ", "
				<< job->sendRequestCount_
				<< "), jobId=" << jobId
				<< ", taskId=" << taskId
				<< ", input=" << inputId
				<< ", isComplete=" << (int)isComplete
				<< ", jobExecutionCount=" << job->executionCount_);

			ec.getEngine().addTimer(ev, waitTime);
			return;
		}
		recvPartitionId = job->getPartitionId();
		if (isEmpty || isComplete) {
			TupleList::Block block;
			job->request(&ec, &ev, controlType,
				taskId, inputId, isEmpty, isComplete, &block, emTime);
		}
		else {
			Task* task = NULL;
			TupleList::Block block;
			bool isExistBlock = false;
			bool isExists = job->getTaskBlock(taskId, inputId, task, block, isExistBlock);
			if (isExists) {
				job->request(&ec, &ev, controlType,
					taskId, inputId, isEmpty, isComplete, &block, emTime);
			}
			else {
				if (!isExistBlock) {
					job->checkNextEvent(ec, controlType, taskId, inputId);
				}
				else {
				}
			}
		}
	}
	catch (std::exception& e) {
		handleError(ec, ev, e, jobId, controlType);
	}
}

void Job::setupTaskInfo(Task* task,
	EventContext& ec, Event& ev, TaskId inputId) {
	task->inputNo_ = inputId;
	task->startTime_ = util::DateTime::now(false).getUnixTime();
	task->cxt_->setEventContext(&ec);
	task->cxt_->setEvent(&ev);
	task->cxt_->setStoreMemoryAgingSwapRate(storeMemoryAgingSwapRate_);
	task->cxt_->setTimeZone(timezone_);
}

bool Job::pipeOrFinish(Task* task, TaskId inputId,
	JobManager::ControlType controlType, TupleList::Block* block) {
	try {
		SQLContext& cxt = *task->cxt_;
		bool remaining;
		if (controlType == FW_CONTROL_PIPE) {
			TRACE_TASK_START(task, inputId, controlType);
			task->startProcessor(JobManager::TASK_EXEC_PIPE);
			remaining = task->processor_->pipe(cxt, inputId, *block);
			checkSchema(*cxt.getEventContext(), task);
		}
		else if (controlType == FW_CONTROL_FINISH) {
			TRACE_TASK_START(task, inputId, controlType);
			task->startProcessor(JobManager::TASK_EXEC_FINISH);
			remaining = task->processor_->finish(cxt, inputId);
		}
		else {
			return true;
		}
		task->endProcessor();
		TRACE_TASK_END(task, inputId, controlType);
		return remaining;
	}
	catch (std::exception& e) {
		task->endProcessor();
		TRACE_TASK_END(task, inputId, controlType);
		removeTask(task->taskId_);
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

bool Job::executeContinuous(
	EventContext& ec, Event& ev, Task* task, TaskId inputId,
	JobManager::ControlType controlType, bool checkRemaining,
	bool isEmpty, bool isComplete, EventMonotonicTime emTime) {
	bool remaining = checkRemaining;
	SQLService* sqlSvc = jobManager_->getSQLService();
	SQLContext& cxt = *task->cxt_;

	TupleList::Block block;

	if (!task->isCompleted() && (remaining ||
		(task->isImmediate() && task->sqlType_ != SQLType::EXEC_DDL))) {
		bool pending = (controlType != FW_CONTROL_NEXT);
		do {
			pending = jobManager_->checkExecutableTask(ec, ev, emTime,
				JobManager::DEFAULT_TASK_INTERRUPTION_INTERVAL, pending);

			if (pending) {

				ControlType nextControlType;
				bool nextEmpty = (remaining || isEmpty);
				if (remaining && !task->isImmediate()) {
					nextControlType = FW_CONTROL_NEXT;
					nextEmpty = true;
				}
				else {
					nextControlType = FW_CONTROL_PIPE;
				}
				TRACE_NEXT_EVENT(task, inputId, nextControlType);
				sendJobEvent(ec, 0, nextControlType,
					NULL, NULL, task->taskId_,
					inputId, task->loadBalance_, task->getServiceType(),
					nextEmpty, isComplete, static_cast<TupleList::Block*>(NULL));
				return false;
			}
			pending = true;
			try {
				TaskExecutionStatus status = (remaining ?
					JobManager::TASK_EXEC_NEXT :
					JobManager::TASK_EXEC_PIPE);
				TRACE_TASK_START(task, inputId, controlType);
				task->startProcessor(status);
				const bool nextRemaining = (remaining ?
					task->processor_->next(cxt) :
					task->processor_->pipe(cxt, inputId, block));
				task->endProcessor();
				TRACE_TASK_END(task, inputId, controlType);
				if (remaining) {
					if (!nextRemaining) {
						break;
					}
					remaining = nextRemaining;
				}
			}
			catch (std::exception& e) {
				task->endProcessor();
				TRACE_TASK_END(task, inputId, controlType);
				removeTask(task->taskId_);
				GS_RETHROW_USER_OR_SYSTEM(e, "");
			}
		} while (!task->isCompleted());
	}
	if (!task->isCompleted()) {

	}
	return true;
}

void Job::doComplete(EventContext& ec, Task* task, TaskId inputId,
	bool isEmpty, bool isComplete) {
	SQLContext& cxt = *task->cxt_;
	if (task->isCompleted() || cxt.isFetchComplete()) {
		if (task->sqlType_ == SQLType::EXEC_RESULT) {
			task->profilerInfo_->complete();
			setProfilerInfo(task);
		}
	}
	int32_t execTaskCount = ++taskCompletedCount_;
	if (execTaskCount == selfTaskNum_ || cxt.isFetchComplete()) {
		if (isCoordinator()) {
			int32_t execJobCount = ++executeCompletedCount_;
			if (isExplainAnalyze_) {
				if (execJobCount == static_cast<int32_t>(nodeList_.size())) {
					if (isImmediateReply(&ec, task->getServiceType())) {
						jobManager_->executeStatementSuccess(ec, this, true, true);
					}
					else {
						sendJobEvent(ec, 0, JobManager::FW_CONTROL_EXECUTE_SUCCESS,
							NULL, NULL, 0, 0, connectedPId_,
							SQL_SERVICE, false, false, static_cast<TupleList::Block*>(NULL));
					}
				}
			}
			else {
				jobManager_->executeStatementSuccess(ec, this, false, false);
			}
		}
		else {
			sendJobEvent(ec, coordinatorNodeId_,
				JobManager::FW_CONTROL_EXECUTE_SUCCESS,
				NULL, NULL, 0, 0, connectedPId_,
				SQL_SERVICE, false, false, static_cast<TupleList::Block*>(NULL));
			jobManager_->remove(jobId_);
		}
		status_ = Job::FW_JOB_COMPLETE;
	}
	else {
		removeTask(task->taskId_);
	}
}

void Job::executeTask(EventContext* ec, Event* ev,
	JobManager::ControlType controlType,
	TaskId taskId, TaskId inputId, bool isEmpty, bool isComplete,
	TupleList::Block* block, EventMonotonicTime emTime,
	int64_t waitTime) {

	const char* taskName = NULL;
	try {
		SQLService* sqlSvc = jobManager_->getSQLService();
		Task* task = getTask(taskId);
		if (task == NULL) {
			return;
		}
		setupTaskInfo(task, *ec, *ev, inputId);
		SQLContext& cxt = *task->cxt_;
		taskName = getProcessorName(task->sqlType_);
		if (task->completed_) {
			return;
		}
		assert(task->processor_);
		bool remaining = pipeOrFinish(
			task, inputId, controlType, block);
		if (isComplete) {
			if (!task->completed_ && task->isDml_) {
				int32_t loadBalance = task->loadBalance_;
				sendJobEvent(*ec, 0, FW_CONTROL_FINISH, NULL, NULL, task->taskId_,
					0, loadBalance, task->getServiceType(),
					isEmpty, true, static_cast<TupleList::Block*>(NULL));
				return;
			}
		}
		if (!executeContinuous(*ec, *ev, task, inputId, controlType,
			remaining, isEmpty, isComplete, emTime)) {
			return;
		}
		if (controlType == FW_CONTROL_NEXT) {
			if (task->isCompleted()) {
				controlType = FW_CONTROL_FINISH;
			}
			else {
				controlType = FW_CONTROL_PIPE;
			}
		}
		if (task->isCompleted() || cxt.isFetchComplete()) {
			doComplete(*ec, task, inputId, isEmpty, isComplete);
		}
		else {
			checkNextEvent(*ec, controlType, taskId, inputId);
		}

		int64_t total = getVarTotalSize(taskId);
		jobAllocateMemory_ = getVarTotalSize();
		checkMemoryLimit();
	}
	catch (std::exception& e) {
		if (taskName == NULL) {
			taskName = "";
		}
		GS_RETHROW_USER_OR_SYSTEM(e,
			"Execute SQL failed (reason=" << GS_EXCEPTION_MESSAGE(e)
			<< " (jobId=" << jobId_ << ", operation=" << getControlName(controlType)
			<< ", taskld=" << taskId << ", processor=" << taskName << ")" << ")");
	}
}

void Job::sendJobInfo(EventContext& ec,
	JobInfo* jobInfo, int64_t waitInterval) {
	util::StackAllocator& alloc = ec.getAllocator();
	if (jobInfo->gsNodeInfoList_.size() > 1) {
		try {
			encodeProcessor(ec, alloc, jobInfo);
			for (size_t pos = 0; pos < jobInfo->gsNodeInfoList_.size(); pos++) {
				GsNodeInfo* nodeInfo = jobInfo->gsNodeInfoList_[pos];
				if (pos == 0) {
					continue;
				}
				else {
					NodeId nodeId = resolveNodeId(nodeInfo);
					util::Random random(jobInfo->startTime_);
					waitInterval = EE_PRIORITY_HIGH;
					sendJobEvent(ec, nodeId, FW_CONTROL_DEPLOY, jobInfo, NULL,
						-1, 0, random.nextInt32(KeyDataStore::MAX_PARTITION_NUM),
						SQL_SERVICE, false, false, (TupleList::Block*)NULL, waitInterval);
					sendJobCount_++;
				}
			}
		}
		catch (std::exception& e) {
			UTIL_TRACE_EXCEPTION(SQL_SERVICE, e, "");

		}
	}
}

void Job::recvDeploySuccess(EventContext& ec, int64_t waitInterval) {
	try {
		EventMonotonicTime emTime = -1;
		if (checkStart()) {
			for (size_t pos = 0; pos < nodeList_.size(); pos++) {
				if (nodeList_[pos] == SELF_NODEID) {
					request(&ec, NULL, FW_CONTROL_PIPE0, -1, 0,
						false, false, (TupleList::Block*)NULL, emTime, waitInterval);
				}
				else {
					sendJobEvent(ec, nodeList_[pos], FW_CONTROL_PIPE0, NULL, NULL,
						-1, 0, 0, SQL_SERVICE, false, false, (TupleList::Block*)NULL, waitInterval);
				}
			}
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void Job::recvExecuteSuccess(
	EventContext& ec, EventByteInStream& in, bool isPending) {
	try {
		if (!isCoordinator()) {
			return;
		}
		util::StackAllocator& alloc = ec.getAllocator();
		JobProfilerInfo profs(alloc);
		if (isExplainAnalyze_) {
			util::ObjectCoder::withAllocator(alloc).decode(in, profs);
		}
		int32_t execJobCount = 0;
		if (!isPending) {
			setProfiler(profs, true);
			execJobCount = ++executeCompletedCount_;
		}
		else {
			execJobCount = executeCompletedCount_;
		}
		if (execJobCount == static_cast<int32_t>(nodeList_.size()) || isPending) {
			jobManager_->executeStatementSuccess(ec, this, isExplainAnalyze_, true);
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void JobManager::Job::forwardRequest(
	EventContext& ec, ControlType controlType,
	Task* task, int outputId, bool isEmpty,
	bool isComplete, TupleList::Block* block) {
	try {
		for (size_t pos = 0; pos <
			task->outputDispatchList_[outputId].size(); pos++) {
			Dispatch* dispatch = task->outputDispatchList_[outputId][pos];
			sendBlock(ec, dispatch->nodeId_, controlType, NULL, NULL,
				dispatch->taskId_, dispatch->inputId_, dispatch->loadBalance_,
				dispatch->type_, isEmpty, isComplete,
				block, task->sendBlockCounter_, task);
		}
		task->sendBlockCounter_++;
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void JobManager::Job::sendBlock(EventContext& ec, NodeId nodeId,
	ControlType controlType,
	JobInfo* jobInfo, std::exception* e,
	TaskId taskId, InputId inputId,
	AssignNo loadBalance, ServiceType type,
	bool isEmpty, bool isComplete,
	TupleList::Block* block, int64_t blockNo, Task* task) {
	JobId& jobId = jobId_;
	try {
		if (nodeId == SELF_NODEID) {
			receiveTaskBlock(ec, taskId, inputId, block,
				0, controlType, isEmpty, true);
		}
		else {
			Event request(ec, DISPATCH_JOB, connectedPId_);
			EventByteOutStream out = request.getOutStream();
			ControlType currentControlType = controlType;
			if (controlType == JobManager::FW_CONTROL_CANCEL) {
				currentControlType = JobManager::FW_CONTROL_ERROR;
			}
			SQLJobHandler::encodeRequestInfo(out, jobId, currentControlType);
			SQLJobHandler::encodeTaskInfo(
				out, taskId, inputId, isEmpty, isComplete);
			out << blockNo;
			out << loadBalance;
			if (controlType == JobManager::FW_CONTROL_PIPE) {
				block->encode(out);
			}
			task->sendEventCount_++;
			task->sendEventSize_ += out.base().position();
			sendEventCount_++;
			sendEventSize_ += out.base().position();

			Event sendAckEvent(ec, SEND_EVENT, task->loadBalance_);
			EventByteOutStream sendOut = sendAckEvent.getOutStream();
			SendEventInfo sendEventInfo(sendOut, jobId, task->taskId_);
			DUMP_SEND_CONTROL("Remote send call, jobId=" << jobId << ", taskId=" << task->taskId_);

			jobManager_->sendEvent(request, SQL_SERVICE, nodeId, 0, &sendAckEvent, true);
			sendReady(taskId);
			jobManager_->sendReady();
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void JobManager::Job::sendJobEvent(EventContext& ec, NodeId nodeId,
	ControlType controlType,
	JobInfo* jobInfo, std::exception* e,
	TaskId taskId, InputId inputId, AssignNo loadBalance,
	ServiceType serviceType, bool isEmpty,
	bool isComplete, TupleList::Block* block, int64_t waitInterval) {
	JobId& jobId = jobId_;
	util::StackAllocator& alloc = ec.getAllocator();
	try {
		PartitionId pId = loadBalance;
		EventType eventType = jobManager_->getEventType(controlType);
		if (controlType == JobManager::FW_CONTROL_CANCEL && nodeId != 0) {
			eventType = CONTROL_JOB;
		}

		if (controlType == JobManager::FW_CONTROL_CHECK_CONTAINER) {
			eventType = CONTROL_JOB;
		}

		Event request(ec, eventType, pId);
		EventByteOutStream out = request.getOutStream();
		SQLJobHandler::encodeRequestInfo(out, jobId, controlType);
		bool isSecondary = false;
		switch (controlType) {
		case JobManager::FW_CONTROL_DEPLOY: {
			util::ObjectCoder().encode(out, jobInfo);
			out << std::pair<const uint8_t*, size_t>(jobInfo->outBuffer_.data(),
				jobInfo->outBuffer_.size());
		}
										  break;
		case JobManager::FW_CONTROL_PIPE:
		case JobManager::FW_CONTROL_FINISH:
		case JobManager::FW_CONTROL_PIPE_FINISH:
		case JobManager::FW_CONTROL_NEXT: {
			isSecondary = true;
			SQLJobHandler::encodeTaskInfo(out, taskId, inputId, isEmpty, isComplete);
			if (block != NULL) {
				if (isEmpty || isComplete) {
				}
				else {
					block->encode(out);
				}
			}
		}
										break;
		case JobManager::FW_CONTROL_ERROR: {
			if (e == NULL) {
				GS_THROW_USER_ERROR(GS_ERROR_JOB_INVALID_CONTROL_INFO, "");
			}
			StatementHandler::StatementExecStatus status
				= StatementHandler::TXN_STATEMENT_ERROR;
			const util::Exception checkException = GS_EXCEPTION_CONVERT(*e, "");
			int32_t errorCode = checkException.getErrorCode();
			if (errorCode == GS_ERROR_TXN_PARTITION_ROLE_UNMATCH
				|| errorCode == GS_ERROR_TXN_PARTITION_STATE_UNMATCH
				|| errorCode == GS_ERROR_TXN_CLUSTER_ROLE_UNMATCH) {
				status = StatementHandler::TXN_STATEMENT_DENY;
			}
			if (errorCode == GS_ERROR_JOB_CANCELLED &&
				!jobManager_->getClusterService()->getManager()->isActive()) {
				if (!isCoordinator()) {
					Event cancelRequest(ec, CONTROL_JOB, IMMEDIATE_PARTITION_ID);
					EventByteOutStream out = cancelRequest.getOutStream();
					ControlType controlType = JobManager::FW_CONTROL_CANCEL;
					SQLJobHandler::encodeRequestInfo(out, jobId, controlType);
					jobManager_->sendEvent(
						cancelRequest, SQL_SERVICE, coordinatorNodeId_, 0, NULL, false);
					jobManager_->remove(jobId);
					return;
				}
			}
			if (errorCode == GS_ERROR_JOB_CANCELLED &&
				!jobManager_->getClusterService()->getManager()->isActive()) {
				try {
					GS_THROW_USER_ERROR(GS_ERROR_TXN_CLUSTER_ROLE_UNMATCH,
						GS_EXCEPTION_MESSAGE(*e));
				}
				catch (std::exception& e2) {
					out << status;
					StatementHandler::encodeException(out, e2, 0);
				}
			}
			else {
				out << status;
				StatementHandler::encodeException(out, *e, 0);
			}
		}
										 break;
		case JobManager::FW_CONTROL_CANCEL:
			setCancel();
			break;
		case JobManager::FW_CONTROL_EXECUTE_SUCCESS: {
			encodeProfiler(out);
		}
												   break;
		case JobManager::FW_CONTROL_DEPLOY_SUCCESS:
		case JobManager::FW_CONTROL_PIPE0:
		case JobManager::FW_CONTROL_HEARTBEAT:
		case JobManager::FW_CONTROL_CHECK_CONTAINER:
		case JobManager::FW_CONTROL_CLOSE: {
		}
										 break;
		default:
			GS_THROW_USER_ERROR(
				GS_ERROR_JOB_INVALID_CONTROL_TYPE, "");
		}
		jobManager_->sendEvent(request, serviceType, nodeId, 0, NULL, isSecondary);
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void JobManager::Job::getProfiler(
	util::StackAllocator& eventStackAlloc,
	TaskProfiler& profile, size_t planId) {
	util::LockGuard<util::Mutex> guard(mutex_);
	profile.copy(
		eventStackAlloc, profilerInfos_.profilerList_[planId]->profiler_);
	profile.address_ = ALLOC_NEW(eventStackAlloc) util::String(
		jobManager_->getPartitionTable()->dumpNodeAddress(
			dispatchNodeIdList_[planId],
			static_cast<ServiceType>(dispatchServiceList_[planId])).c_str(),
		eventStackAlloc);
}

void JobManager::Job::setProfiler(
	JobProfilerInfo& profs, bool isRemote) {
	util::LockGuard<util::Mutex> guard(mutex_);
	for (size_t pos = 0; pos < profs.profilerList_.size(); pos++) {
		TaskId taskId = profs.profilerList_[pos]->taskId_;
		if (isRemote && resultTaskId_ == taskId) {
		}
		else {
			profilerInfos_.profilerList_[taskId]->profiler_.copy(
				jobStackAlloc_, profs.profilerList_[pos]->profiler_);
		}
	}
}

void JobManager::Job::encodeProfiler(EventByteOutStream& out) {
	util::ObjectCoder().encode(out, profilerInfos_);
}

TaskProfilerInfo* JobManager::Job::appendProfiler() {
	TaskProfilerInfo* profilerInfo
		= ALLOC_NEW(jobStackAlloc_) TaskProfilerInfo;
	profilerInfos_.profilerList_.push_back(profilerInfo);
	return profilerInfo;
}

void JobManager::Job::cancel(const Event::Source& source, CancelOption& option) {

	setCancel();
	JobId& jobId = jobId_;
	if (isCoordinator()) {
		for (size_t pos = 0; pos < nodeList_.size(); pos++) {
			NodeId targetNodeId = nodeList_[pos];
			ControlType controlType = JobManager::FW_CONTROL_CANCEL;
			EventType eventType = EXECUTE_JOB;
			Event request(source, eventType, connectedPId_);
			EventByteOutStream out = request.getOutStream();
			SQLJobHandler::encodeRequestInfo(out, jobId, controlType);
			jobManager_->sendEvent(request, SQL_SERVICE, targetNodeId, 0, NULL, false);
		}
	}
	if (!isCoordinator() || option.forced_) {
		jobManager_->remove(jobId);
	}
}

void Task::createProcessor(util::StackAllocator& alloc,
	JobInfo* jobInfo, TaskId taskId, TaskInfo* taskInfo) {
	SQLProcessor::Factory factory;
	if (jobInfo->option_.plan_) {
		jobInfo->option_.planNodeId_ = static_cast<uint32_t>(taskId);
		processor_ = factory.create(
			*cxt_, jobInfo->option_, taskInfo->inputTypeList_);
		setGroup_ = cxt_->isSetGroup();
		if (setGroup_) {
			processorGroupId_ = cxt_->getProcessorGroupId();
		}
	}
	else {
		if (isDQLProcessor(sqlType_)) {
			uint32_t size = 0;
			*jobInfo->inStream_ >> size;
			SQLProcessor::makeCoder(
				util::ObjectCoder().withAllocator(jobStackAlloc_),
				*cxt_, &factory, &taskInfo->inputTypeList_).decode(
					*jobInfo->inStream_, processor_);
			setGroup_ = cxt_->isSetGroup();
			if (setGroup_) {
				processorGroupId_ = cxt_->getProcessorGroupId();
			}
		}
	}
	if (job_->isExplainAnalyze_ && processor_ != NULL) {
		SQLProcessor::Profiler profiler(alloc);
		profiler.setForAnalysis(true);
		processor_->setProfiler(profiler);
	}
}

void Job::Task::setupContextBase(
	SQLContext* cxt, JobManager* jobManager) {
	cxt->setClusterService(jobManager->getClusterService());
	cxt->setPartitionTable(jobManager->getPartitionTable());
	TransactionService* txnSvc = jobManager->getTransactionService();
	if (txnSvc) {
		cxt->setTransactionService(txnSvc);
		cxt->setTransactionManager(txnSvc->getManager());
	}
	cxt->setDataStore(jobManager->getExecutionManager()->getManagerSet()->partitionList_);
	cxt->setExecutionManager(jobManager->getExecutionManager());
	cxt->setJobManager(jobManager);
}

void Job::Task::setupContextTask() {
	cxt_->setClientId(&job_->jobId_.clientId_);
	cxt_->setExecId(job_->jobId_.execId_);
	cxt_->setVersionId(job_->jobId_.versionId_);
	cxt_->setStoreMemoryAgingSwapRate(job_->storeMemoryAgingSwapRate_);
	cxt_->setTask(this);
	cxt_->setSyncContext(job_->syncContext_);
	cxt_->setTimeZone(job_->timezone_);
	if (job_->isSetJobTime_) {
		cxt_->setJobStartTime(job_->startTime_);
	}
}

void Job::Task::setupProfiler(TaskInfo* taskInfo) {
	profilerInfo_ = job_->appendProfiler();
	int32_t workerId;
	if (type_ == static_cast<uint8_t>(SQL_SERVICE)) {
		workerId = jobManager_->getSQLService()->getPartitionGroupId(loadBalance_);
	}
	else {
		workerId = jobManager_->getTransactionService()->getPartitionGroupId(loadBalance_)
			+ jobManager_->getSQLService()->getConcurrency();
	}
	profilerInfo_->taskId_ = taskId_;
	profilerInfo_->profiler_.worker_ = workerId;
	if (job_->isExplainAnalyze() || jobManager_->getSQLService()->isEnableProfiler()) {
		profilerInfo_->init(jobStackAlloc_, taskId_,
			taskInfo->inputList_.size(), workerId, true);
	}
}

void Job::Task::appendBlock(InputId inputId, TupleList::Block& block) {
	if (tempTupleList_.size() <= inputId) {
		GS_THROW_USER_ERROR(
			GS_ERROR_SQL_PROC_INTERNAL_INVALID_INPUT, "");
	}
	tempTupleList_[inputId]->append(block);
}

bool Job::Task::getBlock(InputId inputId, TupleList::Block& block) {
	if (blockReaderList_.size() <= inputId) {
		GS_THROW_USER_ERROR(
			GS_ERROR_SQL_PROC_INTERNAL_INVALID_INPUT, "");
	}
	bool retFlag = blockReaderList_[inputId]->next(block);
	if (retFlag) {
		profilerInfo_->incInputCount(inputId, TupleList::tupleCount(block));
	}
	return retFlag;
}

std::string JobManager::Job::Task::dump() {
	util::NormalOStringStream strstrm;
	strstrm << getProcessorName(sqlType_) << ","
		<< taskId_ << "," << counter_
		<< "," << getTaskExecStatusName(status_);
	return strstrm.str().c_str();
}

void ExecuteJobHandler::executeDeploy(EventContext& ec, Event& ev,
	EventByteInStream& in, JobId& jobId,
	int32_t sqlVersionId, PartitionId& recvPartitionId) {
	util::StackAllocator& alloc = ec.getAllocator();

	NodeId nodeId = ClusterService::resolveSenderND(ev);
	if (nodeId == 0) {
		GS_THROW_USER_ERROR(
			GS_ERROR_JOB_MANAGER_INVALID_PROTOCOL,
			"Control deploy must be participant node");
	}
	try {
		JobInfo* jobInfo = ALLOC_NEW(alloc) JobInfo(alloc);
		util::ObjectCoder::withAllocator(alloc).decode(in, jobInfo);
		jobInfo->setup(jobInfo->queryTimeout_,
			jobManager_->getSQLService()->getEE()->getMonotonicTime(),
			jobInfo->startTime_,
			jobInfo->isSetJobTime_, false);
		jobInfo->coordinatorNodeId_ = nodeId;
		jobInfo->inStream_ = &in;
		JobManager::Latch latch(
			jobId, "ExecuteJobHandler::executeDeploy", jobManager_, JobManager::Latch::LATCH_CREATE, jobInfo);
		Job* job = latch.get();
		if (job) {
			job->deploy(ec, alloc, jobInfo, nodeId, 0);
			recvPartitionId = job->getPartitionId();
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void ExecuteJobHandler::executeError(
	EventContext& ec, EventByteInStream& in, JobId& jobId) {
	util::StackAllocator& alloc = ec.getAllocator();
	try {
		JobManager::Latch latch(jobId, "ExecuteJobHandler::executeError", jobManager_);
		Job* job = latch.get();
		if (job == NULL) {
			return;
		}

		StatementHandler::StatementExecStatus status;
		in >> status;
		util::Exception exception;
		SQLErrorUtils::decodeException(alloc, in, exception);
		try {
			throw exception;
		}
		catch (std::exception& e) {
			jobManager_->executeStatementError(ec, &e, jobId);
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void ExecuteJobHandler::executeCancel(EventContext& ec,
	EventByteInStream& in, JobId& jobId) {
	util::StackAllocator& alloc = ec.getAllocator();
	EventMonotonicTime emTime = ec.getEngine().getMonotonicTime();
	try {
		JobManager::Latch latch(jobId, "ExecuteJobHandler::executeCancel", jobManager_);
		Job* job = latch.get();
		if (job == NULL) {
			GS_TRACE_DEBUG(SQL_SERVICE,
				GS_TRACE_SQL_EXECUTION_INFO, "executeCancel:jobId=" << jobId);
			return;
		}
		job->setCancel();
		if (job->isCoordinator()) {
			try {
				RAISE_OPERATION(SQLFailureSimulator::TARGET_POINT_7);
				job->checkCancel("job", false);
			}
			catch (std::exception& e) {
				jobManager_->executeStatementError(ec, &e, jobId);
			}
		}
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void ExecuteJobHandler::executeRecvAck(EventContext& ec, Event& ev,
	EventByteInStream& in, JobId& jobId) {
	util::StackAllocator& alloc = ec.getAllocator();
	try {
		NodeId nodeId = ClusterService::resolveSenderND(ev);
		EventMonotonicTime emTime = ec.getEngine().getMonotonicTime();
		const char* label = "ExecuteJobHandler::executeRecvAck";
		JobManager::Latch latch(jobId, label, jobManager_);
		Job* job = latch.get();
		if (job == NULL) {
			GS_TRACE_DEBUG(SQL_INTERNAL,
				GS_TRACE_SQL_EXECUTION_INFO, "ExecuteRecvAck:jobId=" << jobId);
			return;
		}
		job->recvExecuteSuccess(ec, in, (nodeId == 0));
	}
	catch (std::exception& e) {
		GS_RETHROW_USER_OR_SYSTEM(e, "");
	}
}

void ExecuteJobHandler::operator()(EventContext& ec, Event& ev) {
	JobId jobId;
	JobManager::ControlType controlType;
	NodeId nodeId = 0;
	int32_t sqlVersionId;
	PartitionId recvPartitionId = ev.getPartitionId();
	EventStart eventStart(ec, ev,
		jobManager_->getTransactionService()->getManager()->getEventMonitor(),
		(getServiceType() == TRANSACTION_SERVICE));
	try {
		EventByteInStream in(ev.getInStream());
		decodeRequestInfo(in, jobId, controlType, sqlVersionId);
		switch (controlType) {
		case JobManager::FW_CONTROL_DEPLOY:
			executeDeploy(ec, ev, in, jobId, sqlVersionId, recvPartitionId);
			break;
		case JobManager::FW_CONTROL_PIPE:
		case JobManager::FW_CONTROL_FINISH:
		case JobManager::FW_CONTROL_PIPE_FINISH:
		case JobManager::FW_CONTROL_NEXT: {
			executeRequest(ec, ev, in, jobId, controlType);
		}
										break;
		case JobManager::FW_CONTROL_ERROR: {
			executeError(ec, in, jobId);
		}
										 break;
		case JobManager::FW_CONTROL_CANCEL: {
			executeCancel(ec, in, jobId);
		}
										  break;
		case JobManager::FW_CONTROL_EXECUTE_SUCCESS: {
			executeRecvAck(ec, ev, in, jobId);
		}
												   break;
		default:
			GS_THROW_USER_ERROR(
				GS_ERROR_JOB_INVALID_CONTROL_TYPE, "");
		}
	}
	catch (std::exception& e) {
		handleError(ec, ev, e, jobId, controlType);
	}
}

void ControlJobHandler::operator()(EventContext& ec, Event& ev) {
	JobId jobId;
	JobManager::ControlType controlType = JobManager::FW_CONTROL_UNDEF;
	try {
		PartitionGroupId pgId = ec.getWorkerId();
		util::StackAllocator& alloc = ec.getAllocator();
		NodeId nodeId = ClusterService::resolveSenderND(ev);
		EventByteInStream in(ev.getInStream());
		int32_t sqlVersionId;
		decodeRequestInfo(in, jobId, controlType, sqlVersionId);
		JobManager::Latch latch(jobId, "ControlJobHandler::operator", jobManager_);
		Job* job = latch.get();
		if (job == NULL) {
			return;
		}
		switch (controlType) {
		case JobManager::FW_CONTROL_DEPLOY_SUCCESS:
			job->recvDeploySuccess(ec);
			break;
		case JobManager::FW_CONTROL_PIPE0:
		{
			EventMonotonicTime emTime = -1;
			job->request(&ec, NULL,
				controlType, -1, 0, false, false,
				static_cast<TupleList::Block*>(NULL), emTime);
		}
		break;
		case JobManager::FW_CONTROL_CANCEL:
			job->setCancel();
			jobManager_->cancel(ec, jobId, true);
			break;
		case JobManager::FW_CONTROL_CHECK_CONTAINER:
			job->ackResult(ec);
			break;
		case JobManager::FW_CONTROL_HEARTBEAT:
			break;
		default:
			GS_THROW_USER_ERROR(
				GS_ERROR_JOB_INVALID_CONTROL_TYPE, "");
		}
	}
	catch (std::exception& e) {
		UTIL_TRACE_EXCEPTION_INFO(SQL_SERVICE, e, "");
		handleError(ec, ev, e, jobId, controlType);
	}
}

void SQLResourceCheckHandler::operator() (EventContext& ec, Event& ev) {
	try {
		util::StackAllocator& alloc = ec.getAllocator();
		util::StackAllocator::Scope scope(alloc);
		EventType eventType = ev.getType();
		switch (eventType) {
		case REQUEST_CANCEL: {
			sqlSvc_->requestCancel(ec);
		}
						   break;
		case CHECK_TIMEOUT_JOB: {
			int64_t currentTime = ec.getEngine().getMonotonicTime();
			executionManager_->checkTimeout(ec);
			jobManager_->checkTimeout(ec, currentTime);
		}
							  break;
		}
	}
	catch (std::exception& e) {
		UTIL_TRACE_EXCEPTION(SQL_SERVICE, e, "");
	}
}

std::ostream& operator<<(std::ostream& stream,
	const JobManager::JobId& id) {
	stream << id.clientId_;
	stream << ":";
	stream << id.execId_;
	stream << ":";
	stream << static_cast<int32_t>(id.versionId_);
	return stream;
}

TaskContext::TaskContext() :
	alloc_(NULL), globalVarAlloc_(NULL),
	jobManager_(NULL), task_(NULL),
	eventContext_(NULL), counter_(0), syncContext_(NULL) {
}

int64_t TaskContext::getMonotonicTime() {
	jobManager_->getSQLService()->checkActiveStatus();
	return jobManager_->getSQLService()->getEE()->getMonotonicTime();
}

NoSQLSyncContext* TaskContext::getSyncContext() const {
	return syncContext_;
}

void TaskContext::setSyncContext(NoSQLSyncContext* syncContext) {
	syncContext_ = syncContext;
}

util::StackAllocator& TaskContext::getAllocator() {
	return *alloc_;
}

void TaskContext::setAllocator(util::StackAllocator* alloc) {
	alloc_ = alloc;
}

EventContext* TaskContext::getEventContext() const {
	return eventContext_;
}

void TaskContext::setEventContext(EventContext* eventContext) {
	eventContext_ = eventContext;
}

void TaskContext::setJobManager(JobManager* jobManager) {
	jobManager_ = jobManager;
}

Task* TaskContext::getTask() {
	return task_;
}

void TaskContext::setTask(Task* task) {
	task_ = task;
}

void TaskContext::transfer(TupleList::Block& block) {
	assert(eventContext_);
	checkCancelRequest();
	task_->job_->forwardRequest(*eventContext_,
		JobManager::FW_CONTROL_PIPE,
		task_, 0, false, false, (TupleList::Block*)&block);
}

void TaskContext::transfer(TupleList::Block& block,
	bool completed, int32_t outputId) {
	assert(eventContext_);
	checkCancelRequest();
	task_->job_->forwardRequest(*eventContext_,
		JobManager::FW_CONTROL_PIPE,
		task_, outputId,
		false, completed, (TupleList::Block*)&block);
	task_->setCompleted(completed);
}

void TaskContext::finish() {

	checkCancelRequest();
	finish(0);
}

void TaskContext::finish(int32_t outputId) {
	task_->job_->setProfilerInfo(task_);
	task_->job_->forwardRequest(*eventContext_,
		JobManager::FW_CONTROL_FINISH,
		task_, outputId, true, true, (TupleList::Block*)NULL);
	task_->completed_ = true;
	TRACE_TASK_COMPLETE(task_, outputId);
}

TaskOption::TaskOption(util::StackAllocator& alloc) :
	plan_(NULL),
	planNodeId_(0),
	byteInStream_(NULL),
	inStream_(NULL),
	baseStream_(NULL),
	jsonValue_(NULL) {
	static_cast<void>(alloc);
}

void SQLJobHandler::initialize(const ManagerSet& mgrSet, ServiceType type) {
	try {
		pt_ = mgrSet.pt_;
		executionManager_ = mgrSet.execMgr_;
		jobManager_ = mgrSet.jobMgr_;
		clsSvc_ = mgrSet.clsSvc_;
		sqlSvc_ = mgrSet.sqlSvc_;
		type_ = type;
	}
	catch (std::exception& e) {
		GS_RETHROW_SYSTEM_ERROR(e, "SQL service handler initialize failed");
	}
}

void TaskContext::checkCancelRequest() {
	RAISE_OPERATION(SQLFailureSimulator::TARGET_POINT_8);
	task_->job_->checkCancel("transfer", false);
}

void JobInfo::setup(int64_t expiredTime, int64_t elapsedTime,
	int64_t currentClockTime, bool isSetJobTime, bool isCoordinator) {
	startTime_ = currentClockTime;
	startMonotonicTime_ = elapsedTime;
	isSetJobTime_ = isSetJobTime;
	queryTimeout_ = expiredTime;
	coordinator_ = isCoordinator;
	if (expiredTime == INT64_MAX) {
		expiredTime_ = expiredTime;
	}
	else {
		if (elapsedTime >= (INT64_MAX - expiredTime
			- JobManager::JOB_TIMEOUT_SLACK_TIME)) {
			expiredTime_ = INT64_MAX;
		}
		else {
			expiredTime_ = elapsedTime + expiredTime
				+ JobManager::JOB_TIMEOUT_SLACK_TIME;
		}
	}
}

TaskInfo* JobInfo::createTaskInfo(SQLPreparedPlan* pPlan, size_t pos) {
	TaskInfo* taskInfo = ALLOC_NEW(eventStackAlloc_) TaskInfo(eventStackAlloc_);
	taskInfo->setPlanInfo(pPlan, pos);
	taskInfo->taskId_ = static_cast<int32_t>(pos);
	taskInfoList_.push_back(taskInfo);
	return taskInfo;
}

GsNodeInfo* JobInfo::createAssignInfo(
	PartitionTable* pt, NodeId nodeId,
	bool isLocal, JobManager* jobManager) {

	GsNodeInfo* assignedInfo = ALLOC_NEW(eventStackAlloc_)
		GsNodeInfo(eventStackAlloc_);

	if (!isLocal) {
		if (nodeId == 0) {
			assignedInfo->address_ = jobManager->getHostAddress().c_str();
		}
		else {
			assignedInfo->address_ = pt->getNodeAddress(nodeId,
				SQL_SERVICE).toString(false).c_str();
		}
		assignedInfo->port_
			= pt->getNodeAddress(nodeId, SQL_SERVICE).port_;
	}
	assignedInfo->nodeId_ = nodeId;
	gsNodeInfoList_.push_back(assignedInfo);
	return assignedInfo;
}

void DispatchJobHandler::operator()(EventContext& ec, Event& ev) {

	JobId jobId;
	TaskId taskId = 0;
	InputId inputId = 0;
	JobManager::ControlType controlType = JobManager::FW_CONTROL_UNDEF;

	try {
		util::StackAllocator& alloc = ec.getAllocator();
		EventByteInStream in(ev.getInStream());
		bool isLocal = false;
		if (ev.getSenderND().isEmpty()) {
			isLocal = true;
		}
		int32_t sqlVersionId;
		decodeRequestInfo(in, jobId, controlType, sqlVersionId);
		bool isEmpty = false;
		bool isComplete = false;
		int64_t blockNo = 0;
		decodeTaskInfo(in, taskId, inputId, isEmpty, isComplete, blockNo);

		JobManager::Latch latch(jobId, "DispatchJobHandler::operator", jobManager_);
		Job* job = latch.get();
		if (job == NULL) {
			return;
		}
		if (controlType == JobManager::FW_CONTROL_ERROR
			|| controlType == JobManager::FW_CONTROL_CANCEL) {
			job->setCancel();
		}
		RAISE_OPERATION(SQLFailureSimulator::TARGET_POINT_9);
		job->checkCancel("dispatch", false);

		int32_t loadBalance;
		in >> loadBalance;
		PartitionId pId = loadBalance;
		ServiceType serviceType = SQL_SERVICE;

		LocalTempStore& store = jobManager_->getStore();
		TupleList::Block block(store, store.getDefaultBlockSize(),
			LocalTempStore::UNDEF_GROUP_ID);

		if (!isEmpty && !isComplete) {
			block.decode(alloc, in);
		}
		job->receiveTaskBlock(ec, taskId, inputId, &block, blockNo,
			controlType, isEmpty, false);
	}
	catch (std::exception& e) {
		handleError(ec, ev, e, jobId, controlType);
	}
}

void JobManager::Job::receiveNoSQLResponse(EventContext& ec,
	util::Exception& dest, StatementExecStatus status, int32_t pos) {
	util::LockGuard<util::Mutex> guard(mutex_);
	if (baseTaskId_ == UNDEF_TASKID || taskList_.size() <= baseTaskId_) {
		GS_THROW_USER_ERROR(GS_ERROR_JOB_INTERNAL, "");
	}
	if (taskList_[baseTaskId_]) {
		ackDDLStatus(pos);
	}
	if (status != StatementHandler::TXN_STATEMENT_SUCCESS) {
		bool isContinue = false;
		try {
			throw dest;
		}
		catch (std::exception& e) {
			const util::Exception checkException = GS_EXCEPTION_CONVERT(e, "");
			int32_t errorCode = checkException.getErrorCode();
			if (errorCode == GS_ERROR_DS_CONTAINER_UNEXPECTEDLY_REMOVED
				|| errorCode == GS_ERROR_TXN_CONTAINER_NOT_FOUND
				|| errorCode == GS_ERROR_DS_DS_CONTAINER_EXPIRED) {
				isContinue = true;
			}
			if (!isContinue) {
				handleError(ec, &e);
			}
		}
		if (!isContinue)
			return;
	}
	Event execEv(ec, EXECUTE_JOB, connectedPId_);
	EventByteOutStream out = execEv.getOutStream();
	ControlType controlType = JobManager::FW_CONTROL_PIPE;
	SQLJobHandler::encodeRequestInfo(out, jobId_, controlType);
	int32_t inputId = 0;
	bool isEmpty = true;
	bool isComplete = false;
	SQLJobHandler::encodeTaskInfo(out, baseTaskId_, inputId, isEmpty, isComplete);
	jobManager_->addEvent(execEv, SQL_SERVICE);
}

void JobId::parse(util::StackAllocator& alloc,
	util::String& jobIdStr, bool isJobId) {
	const char* separator = ":";
	size_t pos1 = jobIdStr.find(separator);
	if (pos1 != 36) {
		GS_THROW_USER_ERROR(GS_ERROR_JOB_INVALID_JOBID_FORMAT,
			"Invalid format : " << jobIdStr
			<< ", expected uuid length=36, actual=" << pos1);
	}
	util::String uuidString(jobIdStr.c_str(), 36, alloc);
	if (UUIDUtils::parse(uuidString.c_str(), clientId_.uuid_) != 0) {
		GS_THROW_USER_ERROR(GS_ERROR_JOB_INVALID_JOBID_FORMAT,
			"Invalid format : " << jobIdStr << ", invalid uuid format");
	}
	size_t pos2 = jobIdStr.find(separator, pos1 + 1);
	if (std::string::npos == pos2) {
		GS_THROW_USER_ERROR(GS_ERROR_JOB_INVALID_JOBID_FORMAT,
			"Invalid format : " << jobIdStr << ", invalid jobId format");
	}
	util::String sessionIdString(jobIdStr, pos1 + 1,
		pos2 - pos1 - 1, alloc);
	clientId_.sessionId_ = atoi(sessionIdString.c_str());
	size_t pos3 = jobIdStr.find(separator, pos2 + 1);
	if (std::string::npos == pos3) {
		GS_THROW_USER_ERROR(GS_ERROR_JOB_INVALID_JOBID_FORMAT,
			"Invalid format : " << jobIdStr << ", invalid jobId format");
	}
	util::String execIdString(jobIdStr, pos2 + 1,
		pos3 - pos2 - 1, alloc);
	execId_ = atoi(execIdString.c_str());
	size_t pos4 = jobIdStr.find(separator, pos3 + 1);
	if (std::string::npos != pos4) {
		GS_THROW_USER_ERROR(GS_ERROR_JOB_INVALID_JOBID_FORMAT,
			"Invalid format : " << jobIdStr << ", invalid jobId format");
	}
	util::String versionIdString(jobIdStr, pos3 + 1,
		jobIdStr.size() - pos3 - 1, alloc);
	versionId_ = atoi(versionIdString.c_str());
}

void JobId::toString(util::StackAllocator& alloc,
	util::String& str, bool isClientIdOnly) {
	util::NormalOStringStream strstrm;
	char tmpBuffer[37];
	UUIDUtils::unparse(clientId_.uuid_, tmpBuffer);
	util::String tmpUUIDStr(tmpBuffer, 36, alloc);
	strstrm << tmpUUIDStr.c_str();
	strstrm << ":";
	strstrm << clientId_.sessionId_;
	if (!isClientIdOnly) {
		strstrm << ":";
		strstrm << execId_;
		strstrm << ":";
		strstrm << static_cast<int32_t>(versionId_);
	}
	str = strstrm.str().c_str();
}

void Job::setProfilerInfo(Task* task) {
	if (isExplainAnalyze()) {
		task->profilerInfo_->complete();
		util::LockGuard<util::Mutex> guard(mutex_);
		task->profilerInfo_->setCustomProfile(task->processor_);
	}
}

void SQLJobHandler::handleError(EventContext& ec, Event& ev,
	std::exception& e, JobId& jobId, ControlType controlType) {

	try {
		if (!jobId.isValid()) {
			UTIL_TRACE_EXCEPTION_WARNING(SQL_SERVICE, e, "");
			return;
		}
		JobManager::Latch latch(jobId, "SQLJobHandler::handleError", jobManager_);
		Job* job = latch.get();
		if (job != NULL) {
			try {
				job->handleError(ec, &e);
			}
			catch (std::exception& e2) {
				GS_RETHROW_USER_OR_SYSTEM(e, "");
			}
		}
	}
	catch (std::exception& e) {
		UTIL_TRACE_EXCEPTION_WARNING(SQL_SERVICE, e, "");
		if (GS_EXCEPTION_CHECK_CRITICAL(e)) {
			clsSvc_->setError(ec, &e);
		}
	}
}

void SQLJobHandler::decodeRequestInfo(EventByteInStream& in,
	JobId& jobId, JobManager::ControlType& controlType,
	int32_t& sqlVersionId) {
	in >> sqlVersionId;
	in.readAll(jobId.clientId_.uuid_, TXN_CLIENT_UUID_BYTE_SIZE);
	in >> jobId.clientId_.sessionId_;
	in >> jobId.execId_;
	in >> jobId.versionId_;
	int8_t tmp;
	in >> tmp;
	controlType = static_cast<JobManager::ControlType>(tmp);
	SQLService::checkVersion(sqlVersionId);
}

void SQLJobHandler::decodeTaskInfo(EventByteInStream& in,
	TaskId& taskId, InputId& inputId,
	bool& isEmpty, bool& isComplete, int64_t& blockNo) {
	in >> taskId;
	in >> inputId;
	StatementHandler::decodeBooleanData<util::ArrayByteInStream>(in, isEmpty);
	StatementHandler::decodeBooleanData<util::ArrayByteInStream>(in, isComplete);
	if (blockNo != -1) {
		in >> blockNo;
	}
}

void SQLJobHandler::encodeRequestInfo(EventByteOutStream& out,
	JobId& jobId, ControlType controlType) {
	out << static_cast<int32_t>(SQLService::SQL_MSG_VERSION);
	out << std::pair<const uint8_t*, size_t>(
		jobId.clientId_.uuid_, TXN_CLIENT_UUID_BYTE_SIZE);
	out << jobId.clientId_.sessionId_;
	out << jobId.execId_;
	out << jobId.versionId_;
	const uint8_t tmp = static_cast<uint8_t>(controlType);
	out << tmp;
}

void SQLJobHandler::encodeTaskInfo(EventByteOutStream& out,
	TaskId taskId, TaskId inputId, bool isEmpty, bool isComplete) {
	out << taskId;
	out << inputId;
	StatementHandler::encodeBooleanData<EventByteOutStream>(out, isEmpty);
	StatementHandler::encodeBooleanData<EventByteOutStream>(out, isComplete);
}

void SQLJobHandler::encodeErrorInfo(EventByteOutStream& out,
	std::exception& e, StatementExecStatus status) {
	const util::Exception checkException = GS_EXCEPTION_CONVERT(e, "");
	out << status;
	StatementHandler::encodeException(out, e, 0);
}

void Job::decodeOptional(EventByteInStream& in) {
	if (in.base().remaining() > 0) {
		in >> storeMemoryAgingSwapRate_;
	}
	if (in.base().remaining() > 0) {
		int64_t offset;
		in >> offset;
		timezone_.setOffsetMillis(offset);
	}
}

bool Job::isImmediateReply(EventContext* ec, ServiceType serviceType) {
	return (
		serviceType == SQL_SERVICE &&
		jobManager_->getSQLService()->getPartitionGroupId(connectedPId_)
		== ec->getWorkerId());
}

void TaskProfilerInfo::init(util::StackAllocator& alloc, int32_t taskId,
	int32_t size, int32_t workerId, bool enableTimer) {
	taskId_ = taskId;
	profiler_.worker_ = workerId;
	profiler_.rows_ = ALLOC_NEW(alloc) util::Vector<int64_t>(size, 0, alloc);
	profiler_.customData_ = ALLOC_NEW(alloc) util::XArray<uint8_t>(alloc);
	enableTimer_ = enableTimer;
	if (enableTimer) {
		watch_.reset();
	}
}

void TaskProfilerInfo::incInputCount(int32_t inputId, int32_t size) {
	if (enableTimer_) {
		(*profiler_.rows_)[inputId] += size;
	}
}

void TaskProfilerInfo::start() {
	if (enableTimer_) {
		watch_.reset();
		watch_.start();
		if (profiler_.executionCount_ == 0) {
			startTime_ = watch_.currentClock();
		}
	}
}

void TaskProfilerInfo::end() {
	profiler_.executionCount_++;
	if (enableTimer_ && !completed_) {
		uint64_t actualTime = watch_.elapsedNanos();
		profiler_.actualTime_ += actualTime;
		watch_.reset();
		watch_.start();
	}
}

void TaskProfilerInfo::complete() {
	if (enableTimer_) {
		if (endTime_ == 0) {
			endTime_ = watch_.currentClock();
		}
		profiler_.leadTime_ = endTime_ - startTime_;
		profiler_.leadTime_ = profiler_.leadTime_ / 1000;
		profiler_.actualTime_ = (profiler_.actualTime_ / 1000 / 1000);
		completed_ = true;
	}
}

void TaskProfilerInfo::setCustomProfile(SQLProcessor* processor) {
	if (processor != NULL) {
		const SQLProcessor::Profiler::StreamData* src =
			processor->getProfiler().getStreamData();
		if (src != NULL) {
			util::XArray<uint8_t>* dest = profiler_.customData_;
			dest->resize(src->size());
			memcpy(dest->data(), src->data(), src->size());
		}
	}
}

bool Job::isDQLProcessor(SQLType::Id type) {
	switch (type) {
	case SQLType::EXEC_RESULT:
	case SQLType::EXEC_INSERT:
	case SQLType::EXEC_UPDATE:
	case SQLType::EXEC_DELETE:
	case SQLType::EXEC_DDL:
		return false;
	default:
		return true;
	}
}

NodeId Job::resolveNodeId(JobInfo::GsNodeInfo* nodeInfo) {
	util::SocketAddress socketAddress(
		nodeInfo->address_.c_str(), nodeInfo->port_);
	const NodeDescriptor& nd
		= jobManager_->getDefaultEE()->getServerND(socketAddress);
	if (nd.isEmpty()) {
		GS_THROW_USER_ERROR(
			GS_ERROR_SQL_COMPILE_INVALID_NODE_ASSIGN,
			"Resolve node failed, jobId=" << jobId_ << ", address=" << socketAddress);
	}
	return static_cast<NodeId>(nd.getId());
}

void Job::setLimitedAssignNoList() {
	if (limitAssignNum_ > jobManager_->getSQLService()->getConcurrency()) {
		limitAssignNum_ = jobManager_->getSQLService()->getConcurrency();
	}
	for (int32_t pos = 0; pos < limitAssignNum_; pos++) {
		limitedAssignNoList_.push_back(jobManager_->getAssignPId());
	}
}


void JobManager::removeExecution(
	EventContext* ec, JobId& jobId,
	SQLExecution* execution, bool withCheck) {
	uint8_t versionId = execution->getContext().getCurrentJobVersionId();
	JobId currentJobId;
	execution->getContext().getCurrentJobId(currentJobId);
	if (jobId == currentJobId) {
		executionManager_->remove(*ec, jobId.clientId_, withCheck, 57, &jobId);
	}
}

util::StackAllocator* Job::getStackAllocator() {
	return jobManager_->getExecutionManager()->getStackAllocator();
}

SQLVarSizeAllocator* Job::getLocalVarAllocator() {
	return ALLOC_VAR_SIZE_NEW(globalVarAlloc_)
		SQLVarSizeAllocator(util::AllocatorInfo(ALLOCATOR_GROUP_SQL_WORK,
			"TaskVarAllocator"));
}

void Job::releaseStackAllocator(util::StackAllocator* alloc) {
	if (alloc) {
		jobManager_->getExecutionManager()->releaseStackAllocator(alloc);
	}
}

void Job::releaseLocalVarAllocator(SQLVarSizeAllocator* varAlloc) {
	if (varAlloc) {
		ALLOC_VAR_SIZE_DELETE(globalVarAlloc_, varAlloc);
	}
}

const SQLProcessorConfig* JobManager::getProcessorConfig() const {
	return executionManager_->getProcessorConfig();
}

void JobManager::getHostInfo() {
	hostName_ = pt_->getNodeAddress(
		SELF_NODEID, SQL_SERVICE).toString(true).c_str();
	hostAddress_ = pt_->getNodeAddress(
		SELF_NODEID, SQL_SERVICE).toString(false).c_str();
	hostPort_ = pt_->getNodeAddress(SELF_NODEID, SQL_SERVICE).port_;
};

bool JobManager::checkExecutableTask(EventContext& ec,
	Event& ev, int64_t startTime, int64_t limitInterval, bool pending) {
	if (!pending) {
		return false;
	}
	int64_t eventCount;
	EventEngine::Tool::getLiveStats(ec.getEngine(), ec.getWorkerId(),
		EventEngine::Stats::EVENT_ACTIVE_EXECUTABLE_COUNT,
		eventCount, &ec, &ev);
	if (eventCount > 0) {
		return true;
	}
	EventEngine::Tool::getLiveStats(
		ec.getEngine(), ec.getWorkerId(),
		EventEngine::Stats::EVENT_CYCLE_HANDLING_AFTER_COUNT,
		eventCount, &ec, &ev);
	if (eventCount > 0) {
		return true;
	}
	sqlSvc_->checkActiveStatus();
	const EventMonotonicTime currentTime =
		ec.getEngine().getMonotonicTime();
	if ((currentTime - startTime) > limitInterval) {
		return true;
	}
	return false;
}

void JobManager::sendEvent(Event& ev,
	ServiceType type, NodeId nodeId, int64_t waitInterval, Event* sendEvent, bool isSecondary) {

	EventEngine* ee = getEE(type);
	if (nodeId == 0) {
		if (waitInterval == 0) {
			ee->add(ev);
		}
		else {
			ee->addTimer(ev, waitInterval);
		}
	}
	else {
		const NodeDescriptor& nd = ee->getServerND(nodeId);
		EventRequestOption sendOption;
		sendOption.timeoutMillis_ = waitInterval;

		if (ev.getType() == CONTROL_JOB) {
			sendOption.timeoutMillis_ = EE_PRIORITY_HIGH;
		}

		if (sendEvent) {
			sendOption.eventOnSent_ = sendEvent;
		}
		ee->send(ev, nd, &sendOption);
	}
}

void JobManager::addEvent(Event& ev, ServiceType type) {
	EventEngine* ee = getEE(type);
	ee->add(ev);
}

void Job::encodeOptional(util::ByteStream<util::XArrayOutStream<> >& out,
	JobInfo* jobInfo) {
	out << jobInfo->storeMemoryAgingSwapRate_;
}

void JobManager::executeStatementError(EventContext& ec,
	std::exception* e, JobId& jobId) {
	util::StackAllocator& alloc = ec.getAllocator();
	SQLExecutionManager::Latch latch(jobId.clientId_,
		executionManager_);
	SQLExecution* execution = latch.get();
	if (execution) {
		RequestInfo request(alloc, false);
		execution->execute(ec, request, true, e, jobId.versionId_, &jobId);
		bool pendingException = execution->getContext().checkPendingException(false);
		cancel(ec, jobId, false);
		remove(jobId);
		if (!pendingException) {
			removeExecution(&ec, jobId, execution, true);
		}
	}
	else {
		remove(jobId);
	}
}

void JobManager::executeStatementSuccess(EventContext& ec, Job* job,
	bool isExplainAnalyze, bool withResponse) {
	JobId& jobId = job->getJobId();

	SQLExecutionManager::Latch latch(jobId.clientId_, getExecutionManager());
	SQLExecution* execution = latch.get();
	if (execution) {
		if (isExplainAnalyze && withResponse) {
			SQLExecution::SQLReplyContext replyCxt;
			replyCxt.setExplainAnalyze(&ec, job, jobId.versionId_, &jobId);
			execution->replyClient(replyCxt);
		}
		cancel(ec, jobId, false);
		remove(jobId);
		removeExecution(&ec, jobId, execution, true);
	}
}

void JobManager::Job::checkDDLPartitionStatus() {
	DDLProcessor* ddlProcessor =
		reinterpret_cast<DDLProcessor*>(taskList_[baseTaskId_]->processor_);
	if (ddlProcessor) {
		ddlProcessor->checkPartitionStatus();
	}
}

void JobManager::Job::ackDDLStatus(int32_t pos) {
	DDLProcessor* ddlProcessor =
		reinterpret_cast<DDLProcessor*>(taskList_[baseTaskId_]->processor_);
	if (ddlProcessor) {
		ddlProcessor->setAckStatus(NULL, pos, 0);
	}
}

bool JobManager::Job::getFetchContext(SQLFetchContext& cxt) {
	Task* resultTask = getTask(resultTaskId_);
	if (resultTask == NULL) {
		return false;
	}
	ResultProcessor* resultProcessor =
		static_cast<ResultProcessor*>(resultTask->processor_);
	cxt.reader_ = resultProcessor->getReader();
	cxt.columnList_ = resultProcessor->getColumnInfoList();
	cxt.columnSize_ = resultProcessor->getColumnSize();
	cxt.isCompleted_ = resultTask->isResultCompleted();
	return true;
}

void JobManager::TaskInfo::setPlanInfo(SQLPreparedPlan* pPlan, size_t pos) {
	if (pos >= pPlan->nodeList_.size()) {
		GS_THROW_USER_ERROR(GS_ERROR_SQL_INTERNAL, "");
	}
	setSQLType(pPlan->nodeList_[pos].type_);
	inputList_ = pPlan->nodeList_[pos].inputList_;
};

AssignNo JobManager::Job::getAssignPId() {
	if (limitAssignNum_ == 0) {
		return jobManager_->getAssignPId();
	}
	else {
		return limitedAssignNoList_[currentAssignNo_++ % limitAssignNum_];
	}
}

int32_t JobManager::getAssignPId() {
	int32_t current = ++currentCnt_;
	if (current >= sqlConcurrency_) {
		currentCnt_ = 0;
		return sqlStartPIdList_[0];
	}
	else {
		return sqlStartPIdList_[current];
	}
}

PartitionGroupId JobManager::getSQLPgId(PartitionId pId) {
	return sqlPgConfig_.getPartitionGroupId(pId);
}

EventType JobManager::getEventType(ControlType type) {
	switch (type) {
	case FW_CONTROL_PIPE:
	case FW_CONTROL_PIPE_FINISH:
	case FW_CONTROL_FINISH:
	case FW_CONTROL_NEXT:
	case FW_CONTROL_ERROR:
	case FW_CONTROL_CANCEL:
	case FW_CONTROL_CLOSE:
	case FW_CONTROL_EXECUTE_SUCCESS:
	case FW_CONTROL_DEPLOY:
		return EXECUTE_JOB;
	case FW_CONTROL_DEPLOY_SUCCESS:
	case FW_CONTROL_PIPE0:
	case FW_CONTROL_HEARTBEAT:
	case FW_CONTROL_CHECK_CONTAINER:
		return CONTROL_JOB;
	default:
		GS_THROW_USER_ERROR(GS_ERROR_JOB_MANAGER_INTERNAL, "");
	}
}

std::string JobId::dump(util::StackAllocator& alloc, bool isClientIdOnly) {
	util::String jobIdStr(alloc);
	toString(alloc, jobIdStr, isClientIdOnly);
	return jobIdStr.c_str();
}

void JobManager::Job::checkNextEvent(EventContext& ec, ControlType controlType,
	TaskId taskId, TaskId inputId) {

	util::LockGuard<util::Mutex> guard(mutex_);
	if (taskList_[taskId] == NULL) {
		return;
	}
	Task* task = taskList_[taskId];
	if (task->blockReaderList_.size() <= inputId) {
		return;
	}
	ControlType nextControlType = controlType;
	bool isEmpty = false;
	bool isComplete = false;

	bool isExistsBlock = task->blockReaderList_[inputId]->isExists();
	bool isPreparedFinished =
		(task->inputStatusList_[inputId] == TASK_INPUT_STATUS_PREPARE_FINISH);
	if (controlType == FW_CONTROL_NEXT) {
		return;
	}
	if (!isExistsBlock) {
		if (isPreparedFinished) {
			nextControlType = FW_CONTROL_FINISH;
			isComplete = true;
			task->inputStatusList_[inputId] = TASK_INPUT_STATUS_FINISHED;
		}
		else {
			return;
		}
	}
	else {
	}

	PartitionId loadBalance = task->loadBalance_;
	ServiceType serviceType = task->getServiceType();
	TRACE_ADD_EVENT("next", jobId_, taskId, inputId, controlType, loadBalance);
	Event request(ec, EXECUTE_JOB, loadBalance);
	EventByteOutStream out = request.getOutStream();
	SQLJobHandler::encodeRequestInfo(out, jobId_, nextControlType);
	SQLJobHandler::encodeTaskInfo(out, taskId, inputId, isEmpty, isComplete);
	jobManager_->addEvent(request, serviceType);
}

bool JobManager::Job::Task::isReadyExecute() {
	if (sendRequestCount_ >= job_->jobManager_->getSQLService()->getSendPendingTaskLimit()) {
		return false;
	}
	else {
		return true;
	}
}

SendEventInfo::SendEventInfo(EventByteInStream& in) {

	in.readAll(jobId_.clientId_.uuid_, TXN_CLIENT_UUID_BYTE_SIZE);
	in >> jobId_.clientId_.sessionId_;
	in >> jobId_.execId_;
	in >> jobId_.versionId_;
	in >> taskId_;
}

SendEventInfo::SendEventInfo(EventByteOutStream& out,
	JobId& jobId, TaskId taskId) {

	out << std::pair<const uint8_t*, size_t>(
		jobId.clientId_.uuid_, TXN_CLIENT_UUID_BYTE_SIZE);
	out << jobId.clientId_.sessionId_;
	out << jobId.execId_;
	out << jobId.versionId_;
	out << taskId;
}

void JobManager::Job::recvSendComplete(TaskId taskId) {

	util::LockGuard<util::Mutex> guard(mutex_);

	Task* task = taskList_[taskId];
	if (task) {
		task->sendComplete();
		sendComplete();
		DUMP_SEND_CONTROL("RECEIVE, jobId=" << jobId_
			<< ", taskId=" << taskId
			<< ", counter=" << task->sendRequestCount_
			<< ", counter1=" << sendRequestCount_);
	}
	else {
		sendComplete();
		DUMP_SEND_CONTROL("RECEIVE2[task removed], jobId="
			<< jobId_ << ", taskId=" << taskId
			<< ", counter1=" << sendRequestCount_);
	}
}

void SendEventHandler::operator()(EventContext& ec, Event& ev) {

	DUMP_SEND_CONTROL("Call SendEvenHandler");

	EventByteInStream in(ev.getInStream());
	SendEventInfo eventInfo(in);

	DUMP_SEND_CONTROL("Send complete, jobId="
		<< eventInfo.jobId_ << ", taskId=" << eventInfo.taskId_);

	try {
		JobManager::Latch latch(eventInfo.jobId_, "SendEventHandler::operator", jobManager_);
		Job* job = latch.get();
		if (job) {
			job->recvSendComplete(eventInfo.taskId_);
		}
		jobManager_->sendComplete();
	}
	catch (std::exception& e) {
		UTIL_TRACE_EXCEPTION_WARNING(SQL_SERVICE, e, "");
	}
}

void JobManager::Job::sendReady(TaskId taskId) {
	util::LockGuard<util::Mutex> guard(mutex_);

	Task* task = taskList_[taskId];
	if (task) {
		task->sendReady();
		DUMP_SEND_CONTROL("[REQUEST], jobId=" << jobId_ << ", taskId=" << taskId
			<< ", counter=" << task->sendRequestCount_ << ", counter1=" << sendRequestCount_);

	}
	sendReady();
}

void JobManager::Job::resetSendCounter() {

	util::LockGuard<util::Mutex> guard(mutex_);
	int64_t prevSendCounter = sendRequestCount_;
	for (size_t pos = 0;pos < taskList_.size(); pos++) {
		Task* task = taskList_[pos];
		if (task) {
			task->sendRequestCount_ = 0;
		}
	}
	sendRequestCount_ = 0;
	DUMP_SEND_CONTROL("[RESET], jobId=" << jobId_
		<< ", prev=" << sendRequestCount_);
}

bool JobManager::Job::checkTaskInterrupt(TaskId taskId) {

	util::LockGuard<util::Mutex> guard(mutex_);
	if (taskId < 0 || taskId >= static_cast<TaskId>(taskList_.size())) {
		return false;
	}
	Task* task = taskList_[taskId];
	return (
		(task && !task->isReadyExecute())
		|| (sendRequestCount_ >= jobManager_->getSQLService()->getSendPendingJobLimit())
		|| (executionCount_ > jobManager_->getSQLService()->getSendPendingTaskConcurrency()));
}

void Job::setJobProfiler(SQLProfilerInfo& prof) {
	prof.deployCompletedCount_ = deployCompletedCount_;
	prof.executeCompletedCount_ = executeCompletedCount_;
}

Job::JobResultRequest::JobResultRequest(
	EventContext& ec, Job* job) :
	ev_(ec, EXECUTE_JOB, job->getConnectedPId()) {

	EventByteOutStream out = ev_.getOutStream();
	SQLJobHandler::encodeRequestInfo(
		out, job->getJobId(), FW_CONTROL_FINISH);

	SQLJobHandler::encodeTaskInfo(
		out, job->getResultTaskId(), 0, true, true);
}

void Job::checkSchema(EventContext& ec, Task* task) {

	if (task->sqlType_ == SQLType::EXEC_SCAN
		&& task->counter_ == 0) {

		if (checkCounter_.notifyLocal()) {
			if (isCoordinator()) {
				ackResult(ec);
			}
			else {
				sendJobEvent(ec, coordinatorNodeId_, FW_CONTROL_CHECK_CONTAINER,
					NULL, NULL, task->taskId_, 0, task->loadBalance_,
					SQL_SERVICE,
					false, false, static_cast<TupleList::Block*>(NULL));
			}
		}
	}
}

void Job::ackResult(EventContext& ec) {

	if (isCoordinator()) {

		if (checkCounter_.notifyGlobal()) {
			JobResultRequest request(ec, this);
			jobManager_->sendEvent(request.getEvent(), SQL_SERVICE,
				SELF_NODEID, 0, NULL, false);
		}
	}
}

void Job::SchemaCheckCounter::initLocal(int32_t targetSize) {
	localTargetCount_ = targetSize;
}

void Job::SchemaCheckCounter::initGlobal(int32_t targetSize) {
	globalTargetCount_ = targetSize;
	if (targetSize > 0) {
		enableFetch_ = false;
	}
}

bool Job::SchemaCheckCounter::notifyLocal() {

	util::LockGuard<util::Mutex> guard(mutex_);
	localCounter_++;
	if (localCounter_ == localTargetCount_) {
		return true;
	}
	else {
		return false;
	}
}

bool Job::SchemaCheckCounter::notifyGlobal() {

	util::LockGuard<util::Mutex> guard(mutex_);

	globalCounter_++;

	if (globalCounter_ == globalTargetCount_) {
		enableFetch_ = true;
		if (pendingFetch_) {
			pendingFetch_ = false;
			return true;
		}
	}
	return false;
}

bool Job::SchemaCheckCounter::isEnableFetch() {

	util::LockGuard<util::Mutex> guard(mutex_);
	return enableFetch_;
}
bool Job::checkAndSetPending() {
	return checkCounter_.checkAndSetPending();
}

bool Job::SchemaCheckCounter::checkAndSetPending() {

	util::LockGuard<util::Mutex> guard(mutex_);
	if (!enableFetch_) {
		pendingFetch_ = true;
	}
	return enableFetch_;
}

bool Job::isEnableFetch() {
	return checkCounter_.isEnableFetch();
}
