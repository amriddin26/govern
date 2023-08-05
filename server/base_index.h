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
/*!
	@file
	@brief Definition of Index base class
*/
#ifndef BASE_INDEX_H_
#define BASE_INDEX_H_

#include "util/trace.h"
#include "data_type.h"
#include "gs_error.h"
#include "object_manager_v4.h"
#include "transaction_context.h"
#include "value.h"			   
#include "value_operator.h"			   
#include <iomanip>
#include <iostream>

struct StringKey;
struct FullContainerKeyAddr;
class TransactionContext;
class BaseContainer;
class TreeFuncInfo;
class CompositeInfoObject;
class ColumnSchema;
struct StringKey;
struct FullContainerKeyAddr;

/*!
	@brief Term Condition
*/
struct TermCondition {
	Operator operator_;		 
	DSExpression::Operation opType_;
	ColumnType valueType_;
	uint32_t columnId_;		 
	const void *value_;   
	uint32_t valueSize_;  

	TermCondition() :
			operator_(NULL),
			opType_(DSExpression::NONE),
			valueType_(COLUMN_TYPE_WITH_BEGIN),
			columnId_(UNDEF_COLUMNID),
			value_(NULL),
			valueSize_(0) {
	}
	TermCondition(
			ColumnType columnType, ColumnType valueType,
			DSExpression::Operation opType, uint32_t columnId,
			const void *value, uint32_t valueSize) :
			opType_(opType),
			valueType_(valueType),
			columnId_(columnId),
			value_(value),
			valueSize_(valueSize) {
		if (columnType == COLUMN_TYPE_COMPOSITE &&
				valueType == COLUMN_TYPE_COMPOSITE) {
			operator_ = NULL;
		}
		else {
			operator_ = ComparatorTable::getOperator(opType, columnType, valueType);
		}
	}
	bool isIncluded() const {
		return DSExpression::isIncluded(opType_);
	}
	bool isStartCondition() const {
		return DSExpression::isStartCondition(opType_);
	}
	bool isEndCondition() const {
		return DSExpression::isEndCondition(opType_);
	}
	bool isRangeCondition() const {
		return (DSExpression::isStartCondition(opType_) ||
			DSExpression::isEndCondition(opType_));
	}
	void copy(util::StackAllocator &alloc, TermCondition &dest) {
		void *newValue = NULL;
		if (value_ != NULL) {
			newValue = alloc.allocate(valueSize_);
			memcpy(newValue, value_, valueSize_);
		}
		dest = TermCondition(valueType_, valueType_, opType_,
			columnId_, newValue, valueSize_);
		dest.operator_ = operator_;
	}
	std::string dump() {
		util::NormalOStringStream strstrm;
		strstrm << "\"TermCondition\" : {";

		strstrm << "{\"opType_\" : " << DSExpression::getOperationStr(opType_) << "}";
		strstrm << ", {\"valueType_\" : " << ValueProcessor::getTypeNameChars(valueType_) << "}";
		strstrm << ", {\"columnId_\" : " << columnId_ << "}";

		strstrm << ", {\"value_\" : ";
		ValueProcessor::dumpSimpleValue(strstrm, valueType_, value_, valueSize_); 
		strstrm << "}";

		strstrm << ", {\"valueSize_\" : " << valueSize_ << "}";
		strstrm << "}";
		return strstrm.str();
	}
};

class TreeFuncInfo {
public:
	TreeFuncInfo(util::StackAllocator &alloc) : alloc_(alloc), columnSchema_(NULL), orgColumnIds_(NULL), fixedAreaSize_(0) {
	}
	void initialize(const util::Vector<ColumnId> &columnIds, const ColumnSchema *srcSchema, bool force = false);
	ColumnSchema *getColumnSchema() {
		return columnSchema_;
	}
	util::Vector<ColumnId> *getOrgColumnIds() {
		return orgColumnIds_;
	}
	CompositeInfoObject *allocateCompositeInfo(util::StackAllocator &alloc);
	CompositeInfoObject *createCompositeInfo(util::StackAllocator &alloc,
	   util::XArray<KeyData> &valueList);
	CompositeInfoObject *createCompositeInfo(util::StackAllocator &alloc, void *data, uint32_t size);
	ColumnId getColumnId(ColumnId orgColumnId);
	uint32_t getFixedAreaSize() const {
		return fixedAreaSize_;
	}
private:
	util::StackAllocator &alloc_;
	ColumnSchema *columnSchema_;
	util::Vector<ColumnId> *orgColumnIds_;
	uint32_t fixedAreaSize_;

};

/*!
	@brief Index base class
*/
class BaseIndex : public BaseObject {
public:
	BaseIndex(TransactionContext &txn, ObjectManagerV4 &objectManager,
		AllocateStrategy &strategy, BaseContainer *container,
		TreeFuncInfo *funcInfo,
		MapType mapType)
		: BaseObject(objectManager, strategy),
		  allocateStrategy_(strategy),
		  container_(container),
		  funcInfo_(funcInfo),
		  mapType_(mapType) {}
	BaseIndex(TransactionContext &txn, ObjectManagerV4 &objectManager, OId oId,
		AllocateStrategy &strategy, BaseContainer *container,
		TreeFuncInfo *funcInfo,
		MapType mapType)
		: BaseObject(objectManager, strategy, oId),
		  allocateStrategy_(strategy),
		  container_(container),
		  funcInfo_(funcInfo),
		  mapType_(mapType) {
		if (funcInfo_ == NULL) {
			util::StackAllocator &alloc = txn.getDefaultAllocator();
			util::Vector<ColumnId> columnIds(alloc);
			columnIds.push_back(UNDEF_COLUMNID);
			funcInfo_ = ALLOC_NEW(alloc) TreeFuncInfo(alloc);
			funcInfo_->initialize(columnIds, NULL);
		}
	}

	virtual bool finalize(TransactionContext &txn) = 0;
	virtual int32_t insert(
		TransactionContext &txn, const void *key, OId oId) = 0;
	virtual int32_t remove(
		TransactionContext &txn, const void *key, OId oId) = 0;
	virtual int32_t update(
		TransactionContext &txn, const void *key, OId oId, OId newOId) = 0;
	MapType getMapType() {return mapType_;}
	static const uint64_t NUM_PER_EXEC = 50;

public:
	virtual ~BaseIndex() {}

	struct SearchContext {
		enum NullCondition {
			IS_NULL,		
			ALL,			
			NOT_IS_NULL		
		};

		enum ResumeStatus {
			NOT_RESUME,		
			NULL_RESUME,	
			NOT_NULL_RESUME	
		};

		enum ConditionType {
			COND_KEY,	
			COND_OTHER,	
			COND_ALL	
		};

		SearchContext(util::StackAllocator &alloc, ColumnId columnId)
			: conditionList_(alloc),
			  keyList_(alloc),
			  limit_(MAX_RESULT_SIZE),
			  isResume_(NOT_RESUME),
			  nullCond_(NOT_IS_NULL),
			  isSuspended_(false),
			  suspendLimit_(MAX_RESULT_SIZE),
			  suspendKey_(NULL),
			  suspendKeySize_(0),
			  suspendValue_(NULL),
			  suspendValueSize_(0),
			  isNullSuspended_(false),
			  suspendRowId_(UNDEF_ROWID),
			  columnIdList_(alloc)
		{
			columnIdList_.push_back(columnId);
		}
		SearchContext(util::StackAllocator &alloc, util::Vector<ColumnId> &columnIds)
			: conditionList_(alloc),
			  keyList_(alloc),
			  limit_(MAX_RESULT_SIZE),
			  isResume_(NOT_RESUME),
			  nullCond_(NOT_IS_NULL),
			  isSuspended_(false),
			  suspendLimit_(MAX_RESULT_SIZE),
			  suspendKey_(NULL),
			  suspendKeySize_(0),
			  suspendValue_(NULL),
			  suspendValueSize_(0),
			  isNullSuspended_(false),
			  suspendRowId_(UNDEF_ROWID),
			  columnIdList_(alloc)
		{
			columnIdList_.assign(columnIds.begin(), columnIds.end());
		}
		SearchContext(util::StackAllocator &alloc, TermCondition &cond, ResultSize limit)
			: conditionList_(alloc),
			  keyList_(alloc),
			  limit_(limit),
			  isResume_(NOT_RESUME),
			  nullCond_(NOT_IS_NULL),
			  isSuspended_(false),
			  suspendLimit_(MAX_RESULT_SIZE),
			  suspendKey_(NULL),
			  suspendKeySize_(0),
			  suspendValue_(NULL),
			  suspendValueSize_(0),
			  isNullSuspended_(false),
			  suspendRowId_(UNDEF_ROWID),
			  columnIdList_(alloc)
		{
			columnIdList_.push_back(cond.columnId_);
			keyList_.push_back(0);
			conditionList_.push_back(cond);
		}

		SearchContext(util::StackAllocator &alloc, TermCondition &startCond,
			TermCondition &endCond, ResultSize limit)
			: conditionList_(alloc),
			  keyList_(alloc),
			  limit_(limit),
			  isResume_(NOT_RESUME),
			  nullCond_(NOT_IS_NULL),
			  isSuspended_(false),
			  suspendLimit_(MAX_RESULT_SIZE),
			  suspendKey_(NULL),
			  suspendKeySize_(0),
			  suspendValue_(NULL),
			  suspendValueSize_(0),
			  isNullSuspended_(false),
			  suspendRowId_(UNDEF_ROWID),
			  columnIdList_(alloc)
		{
			assert(startCond.columnId_ == endCond.columnId_);
			columnIdList_.push_back(startCond.columnId_);
			keyList_.push_back(0);
			keyList_.push_back(1);
			conditionList_.push_back(startCond);
			conditionList_.push_back(endCond);
		}

		uint32_t getConditionNum() const {
			return conditionList_.size();
		}
		uint32_t getRestConditionNum() const {
			return static_cast<uint32_t>(conditionList_.size() - keyList_.size());
		}
		TermCondition &getCondition(uint32_t no) {
			assert(no < getConditionNum());
			return conditionList_[no];
		}
		void addCondition(TransactionContext &txn, TermCondition &term, bool isKey = false);
		void copy(util::StackAllocator &alloc, SearchContext &dest);
		uint32_t getKeyColumnNum() const {
			return columnIdList_.size();
		}
		const util::Vector<ColumnId> &getColumnIds() const {
			return columnIdList_;
		}
		NullCondition getNullCond() {
			return nullCond_;
		}
		void setNullCond(NullCondition cond) {
			if (cond == IS_NULL) {
				keyList_.clear();
			}
			nullCond_ = cond;
		}
		ResultSize getLimit() {
			return limit_;
		}
		void setLimit(ResultSize limit) {
			limit_ = limit;
		}
		void getConditionList(util::Vector<TermCondition> &condList, ConditionType condType);
		bool updateCondition(TransactionContext &txn, TermCondition &newCond);
		ColumnId getScColumnId() const {
			assert(columnIdList_.size() == 1);
			return columnIdList_[0];
		}

		void reserveCondition(size_t size) {
			keyList_.reserve(size);
			columnIdList_.reserve(size);
			conditionList_.reserve(size);
		}

		void setSuspendPoint(
			TransactionContext &txn, const void *suspendKey, uint32_t keySize, OId suspendValue) {
			suspendKey_ = ALLOC_NEW(txn.getDefaultAllocator()) uint8_t[keySize];
			memcpy(suspendKey_, suspendKey, keySize);
			suspendKeySize_ = keySize;

			uint32_t valueSize = sizeof(OId);
			suspendValue_ = ALLOC_NEW(txn.getDefaultAllocator()) uint8_t[valueSize];
			memcpy(suspendValue_, &suspendValue, valueSize);
			suspendValueSize_ = valueSize;
		}
		template <typename K, typename V>
		void setSuspendPoint(
			TransactionContext &txn, ObjectManagerV4 &, AllocateStrategy& strategy, TreeFuncInfo *, const K &suspendKey, const V &suspendValue) {
			UTIL_STATIC_ASSERT((!util::IsSame<K, StringKey>::VALUE));
			UTIL_STATIC_ASSERT((!util::IsSame<K, FullContainerKeyAddr>::VALUE));

			uint32_t keySize = sizeof(K);
			suspendKey_ =
				ALLOC_NEW(txn.getDefaultAllocator()) uint8_t[keySize];
			memcpy(suspendKey_, &suspendKey, keySize);
			suspendKeySize_ = keySize;

			uint32_t valueSize = sizeof(V);
			suspendValue_ = ALLOC_NEW(txn.getDefaultAllocator()) uint8_t[valueSize];
			memcpy(suspendValue_, &suspendValue, valueSize);
			suspendValueSize_ = valueSize;
		}

		bool isNullSuspended() const {
			return isNullSuspended_;
		}
		void setNullSuspended(bool isSuspended) {
			isNullSuspended_ = isSuspended;
		}
		bool isSuspended() const {
			return isSuspended_;
		}
		void setSuspended(bool isSuspended) {
			isSuspended_ = isSuspended;
		}

		ResultSize getSuspendLimit() const {
			return suspendLimit_;
		}
		void setSuspendLimit(ResultSize limit) {
			suspendLimit_ = limit;
		}

		RowId getSuspendRowId() const {
			return suspendRowId_;
		}
		void setSuspendRowId(RowId rowId) {
			suspendRowId_ = rowId;
		}

		uint8_t *getSuspendKey() const {
			return suspendKey_;
		}
		void setSuspendKey(uint8_t *data) {
			suspendKey_ = data;
		}
		uint8_t *getSuspendValue() const {
			return suspendValue_;
		}
		void setSuspendValue(uint8_t *data) {
			suspendValue_ = data;
		}
		uint32_t getSuspendKeySize() const {
			return suspendKeySize_;
		}
		void setSuspendKeySize(uint32_t size) {
			suspendKeySize_ = size;
		}
		uint32_t getSuspendValueSize() const {
			return suspendValueSize_;
		}
		void setSuspendValueSize(uint32_t size) {
			suspendValueSize_ = size;
		}
		ResumeStatus getResumeStatus() const {
			return isResume_;
		}
		void setResumeStatus(ResumeStatus status) {
			isResume_ = status;
		}
		const util::Vector<size_t> &getKeyList() const {
			return keyList_;
		}

		virtual std::string dump();
	protected:
		enum ConditionStatus {
			NO_CONTRADICTION,
			CONTRADICTION,
			REPLACEMENT,
			IGNORANCE
		};
		ConditionStatus checkCondition(TransactionContext &txn, TermCondition &orgCond, TermCondition &newCond);

		util::Vector<TermCondition> conditionList_;  
		util::Vector<size_t> keyList_;  
		ResultSize limit_;				
		ResumeStatus isResume_;
		NullCondition nullCond_;		 

		bool isSuspended_;		   
		ResultSize suspendLimit_;  
		uint8_t *suspendKey_;  
		uint32_t suspendKeySize_;  
		uint8_t *suspendValue_;  
		uint32_t suspendValueSize_;  
		bool isNullSuspended_;
		RowId suspendRowId_;
		util::Vector<ColumnId> columnIdList_; 
	};
	TreeFuncInfo *getFuncInfo() {
		return funcInfo_;
	}

	struct Setting {
		Setting(ColumnType keyType, bool isCaseSensitive, TreeFuncInfo *funcInfo);

		void initialize(util::StackAllocator &alloc, SearchContext &sc,
			OutputOrder outputOrder);
		TermCondition *getStartKeyCondition() const {
			return startCondition_;
		}
		TermCondition *getEndKeyCondition() const {
			return endCondition_;
		}

		bool isCaseSensitive() const {
			return isCaseSensitive_;
		}
		void setCompareNum(uint32_t compareNum) {
			compareNum_ = compareNum;
		}
		uint32_t getCompareNum() const {
			return compareNum_;
		}
		uint32_t getGreaterCompareNum() const {
			return greaterCompareNum_;
		}
		uint32_t getLessCompareNum() const {
			return lessCompareNum_;
		}
		TreeFuncInfo *getFuncInfo() const {
			return funcInfo_;
		}

		util::Vector<TermCondition> *getFilterConds() const {
			return filterConds_;
		}
		std::string dump();

		ColumnType keyType_;
		bool isCaseSensitive_;
		bool isStartIncluded_;
		bool isEndIncluded_;
		uint32_t compareNum_;
		uint32_t greaterCompareNum_;
		uint32_t lessCompareNum_;
		TreeFuncInfo *funcInfo_;
		TermCondition *startCondition_;
		TermCondition *endCondition_;
		util::Vector<TermCondition> *filterConds_;
	};
protected:
	AllocateStrategy &allocateStrategy_;
	BaseContainer *container_;
	TreeFuncInfo *funcInfo_;
	MapType mapType_;

};

typedef BaseIndex::SearchContext::ResumeStatus ResumeStatus;
typedef BaseIndex::SearchContext::NullCondition NullCondition;
typedef BaseIndex::SearchContext BaseSearchContext;

class CompositeInfoObject {
public:
	CompositeInfoObject() {};
	void initialize(util::StackAllocator &alloc, TreeFuncInfo &funcInfo);

	void setKey(TransactionContext &txn, ObjectManagerV4 &objectManager, 
		AllocateStrategy &allocateStrategy, OId neighborOId, 
		TreeFuncInfo *funcInfo, CompositeInfoObject &src);
	void freeKey(ObjectManagerV4 &objectManager, AllocateStrategy &strategy);
	bool isNull(uint32_t pos) const;
	uint8_t *getField(TransactionContext &txn, ObjectManagerV4 &objectManager, AllocateStrategy &strategy, ColumnInfo &columnInfo, VariableArrayCursor *&varCursor) const;
	const uint8_t *getNullsAddr() const;
	const uint8_t *getVarAddr() const;
	const uint8_t *getFixedAddr() const;
	bool hasVariable() const;
	uint8_t getType() const;
	void setType(uint8_t type);
	void setVariableArray(void *value);
	void setFixedField(TreeFuncInfo &funcInfo, uint32_t pos, const void *value);
	void setNull(uint32_t pos);
	void serialize(TransactionContext &txn, ObjectManagerV4 &objectManager, AllocateStrategy &strategy, TreeFuncInfo &funcInfo, uint8_t *&data, uint32_t &size) const;
	void dump(TransactionContext &txn, ObjectManagerV4 &objectManager, AllocateStrategy &strategy, TreeFuncInfo &funcInfo, util::NormalOStringStream &output) const;
	void dump(TreeFuncInfo &funcInfo, util::NormalOStringStream &output) const;

	friend std::ostream &operator<<(std::ostream &out, const CompositeInfoObject &foo) {
		UNUSED_VARIABLE(foo);
		out << "no impl";
		return out;
	}

	static uint32_t calcSize(uint32_t columnNum, uint32_t fixedColumnsSize, bool hasVariable);

private:
	uint8_t getNullBitSize() const;
	void setNullBitSize(uint16_t columnNum);
	void setVariable(bool isVariable);
	uint8_t *getField(ColumnInfo &columnInfo) const;

	static const uint8_t HEADER_SIZE = 1;
	static const uint8_t KEY_ON_MEMORY = 0;
	static const uint8_t KEY_ON_INDEX = 1;
};


template <>
void BaseIndex::SearchContext::setSuspendPoint(TransactionContext &txn,
	ObjectManagerV4 &objectManager, AllocateStrategy &strategy, TreeFuncInfo *funcInfo, const StringKey &suspendKey, const OId &suspendValue);

template <>
void BaseIndex::SearchContext::setSuspendPoint(TransactionContext &txn,
	ObjectManagerV4 &objectManager, AllocateStrategy &strategy, TreeFuncInfo *funcInfo, const FullContainerKeyAddr &suspendKey, const OId &suspendValue);

template <>
void BaseIndex::SearchContext::setSuspendPoint(TransactionContext& txn,
	ObjectManagerV4& objectManager, AllocateStrategy& strategy, TreeFuncInfo* funcInfo, const FullContainerKeyAddr& suspendKey, const KeyDataStoreValue& suspendValue);

inline BaseIndex::SearchContext::ConditionStatus BaseIndex::SearchContext::checkCondition(TransactionContext &txn, 
   TermCondition &orgCond, TermCondition &newCond) {
	ConditionStatus status = NO_CONTRADICTION;
	if (orgCond.columnId_ == newCond.columnId_ && orgCond.valueType_ == newCond.valueType_ &&
		orgCond.isRangeCondition() && newCond.isRangeCondition()) {
		if (orgCond.opType_ == DSExpression::EQ) {
			if (newCond.operator_(txn, static_cast<const uint8_t *>(orgCond.value_), orgCond.valueSize_, 
				static_cast<const uint8_t *>(newCond.value_), newCond.valueSize_)) {
				status = IGNORANCE;
			} else {
				status = CONTRADICTION;
			}
		} else if (newCond.opType_ == DSExpression::EQ) {
			if (orgCond.operator_(txn, static_cast<const uint8_t *>(newCond.value_), newCond.valueSize_, 
				static_cast<const uint8_t *>(orgCond.value_), orgCond.valueSize_)) {
				status = REPLACEMENT;
			} else {
				status = CONTRADICTION;
			}
		} else if ((newCond.isStartCondition() && orgCond.isStartCondition()) ||
			(newCond.isEndCondition() && orgCond.isEndCondition())) { 
			if (orgCond.operator_(txn, static_cast<const uint8_t *>(newCond.value_), newCond.valueSize_, 
				static_cast<const uint8_t *>(orgCond.value_), orgCond.valueSize_)) {
				status = REPLACEMENT;
			} else {
				status = IGNORANCE;
			}
		}
	}
	return status;
}


inline void BaseIndex::SearchContext::addCondition(TransactionContext &txn, TermCondition &term, bool isKey) {
	if (isKey) {
		bool isAllNoContradiction = true;
		ConditionStatus status = NO_CONTRADICTION;
		util::Vector<size_t>::iterator itr;
		for (itr = keyList_.begin(); itr != keyList_.end(); itr++) {
			TermCondition &current = getCondition(*itr);
			status = checkCondition(txn, current, term);
			if (status == REPLACEMENT) {
				current = term;
				isAllNoContradiction = false;
			} else if (status == CONTRADICTION) {
				conditionList_.push_back(term);
				isAllNoContradiction = false;
				break;
			} else if (status == IGNORANCE) {
				isAllNoContradiction = false;
				break;
			}
		}
		if (isAllNoContradiction) {
			keyList_.push_back(static_cast<ColumnId>(conditionList_.size()));
			conditionList_.push_back(term);
		}
	} else {
		conditionList_.push_back(term);
	}
}

#endif  
