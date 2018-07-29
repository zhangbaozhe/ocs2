/*
 * EventTimeIndexer.h
 *
 *  Created on: Jul 4, 2018
 *      Author: farbod
 */

namespace ocs2{

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t STATE_DIM, size_t INPUT_DIM, class LOGIC_RULES_T>
void EventTimeIndexer::set(
		const LogicRulesMachine<STATE_DIM, INPUT_DIM, LOGIC_RULES_T>& logicRulesMachine) {

	// total number of event times
	numEventTimes_ = logicRulesMachine.getLogicRules().eventTimes().size();
	// number of subsystems
	numSubsystems_ = numEventTimes_ + 1;
	// number of partitions
	numPartitions_ = logicRulesMachine.getnumPartitions();
	// number of subsystems in each partition
	numSubsystemsStock_.resize(numPartitions_);
	for (size_t i=0; i<numPartitions_; i++)
		numSubsystemsStock_[i] = logicRulesMachine.getNumEventCounters(i);

	findPartitionsDistribution(numSubsystems_, logicRulesMachine.eventCountersStock_,
			partitionsDistribution_, subsystemsIndeces_);
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
inline const size_t& EventTimeIndexer::numEventTimes() const {

	return numEventTimes_;
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
inline const size_t& EventTimeIndexer::numSubsystems() const {

	return numSubsystems_;
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
inline const size_t& EventTimeIndexer::numPartitionSubsystems(const size_t& partitionIndex) const {

	return numSubsystemsStock_[partitionIndex];
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
inline const EventTimeIndexer::int_array_t& EventTimeIndexer::partitionsDistribution(
		const size_t& subsystemIndex) const {

	return partitionsDistribution_[subsystemIndex];
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
inline const EventTimeIndexer::size_array_t& EventTimeIndexer::subsystemIndeces(
		const size_t& subsystemIndex) const {

	return subsystemsIndeces_[subsystemIndex];
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
inline void EventTimeIndexer::display() const {

	size_t itr;

	itr = 0;
	std::cerr << "Partitions distributions for subsystems:\n\t ";
	for (const auto& partitions : partitionsDistribution_) {  // printing
		std::cerr << itr << ":{";
		itr++;

		for (const auto& pi : partitions) {
			std::cerr << pi << ", ";
		}
		if (!partitions.empty())  std::cerr << "\b\b";
		std::cerr << "},  ";
	}
	std::cerr << std::endl;

	itr = 0;
	std::cerr << "Subsystems indeces in partitions:\n\t ";
	for (const auto& indeces : subsystemsIndeces_) {  // printing
		std::cerr << itr << ":{";
		itr++;

		for (const auto& i : indeces) {
			std::cerr << i << ", ";
		}
		if (!indeces.empty())  std::cerr << "\b\b";
		std::cerr << "},  ";
	}
	std::cerr << std::endl;
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <typename SCALAR_T>
void EventTimeIndexer::findSubsystemActiveTimeIntervals(
		const size_t& subsystemIndex,
		const std::vector<std::vector<SCALAR_T>>& timeTrajectoriesStock,
		const size_array2_t& eventsPastTheEndIndecesStock,
		partition_range_array_t& subsystemRange) const {

	subsystemRange.clear();

	// spanning partitions by the subsystem
	const int_array_t& coveringPartitions = partitionsDistribution_[subsystemIndex];

	// subsystems which are out of the time partitions coverage
	if (coveringPartitions[0]==-1 || coveringPartitions[0]==numPartitions_)
		return;

	// The point is that either subsystemsIndeces_[subsystemIndex has no elements (the above return case)
	// or it has the same number of elements as coveringPartitions.
	for (size_t i=0; i<coveringPartitions.size(); i++) {

		const int& p = coveringPartitions[i];
		const size_t& index = subsystemsIndeces_[subsystemIndex][i];

		// part or all of the subsystem is in an inactive partition
		if (timeTrajectoriesStock[p].empty()) {
			continue;
		}

		// number of active time events in the partition
		const size_t NE = eventsPastTheEndIndecesStock[p].size();

		// if all the events are not active in a partition (numInactiveSubsystems>0)
		// then it means that earlier subsystems are not active
		const int numInactiveSubsystems = numSubsystemsStock_[p] - (NE+1);
		if ((int)index < numInactiveSubsystems) {
			continue;

		} else {
			size_array_t eventsPastTheEndIndeces(NE+2);
			eventsPastTheEndIndeces[0] = 0;
			eventsPastTheEndIndeces[NE+1] = timeTrajectoriesStock[p].size();
			for (size_t j=0; j<NE; j++)
				eventsPastTheEndIndeces[j+1] = eventsPastTheEndIndecesStock[p][j];
			range_t range = std::make_pair( eventsPastTheEndIndeces[index-numInactiveSubsystems],
					eventsPastTheEndIndeces[index+1-numInactiveSubsystems] );
			subsystemRange.emplace_back(p, range);
		}
	} // end of i loop
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
inline void EventTimeIndexer::findPartitionsDistribution(
		const size_t& numSubsystems,
		const size_array2_t& eventCountersStock,
		int_array2_t& partitionsDistribution,
		size_array2_t& subsystemsIndeces) const {

	partitionsDistribution.resize(numSubsystems);
	subsystemsIndeces.resize(numSubsystems);
	size_array_t::const_iterator it;

	int partitionIndex = 0;
	int indexInPartition = -1;

	for (size_t subsystem=0; subsystem<numSubsystems; subsystem++) {

		partitionsDistribution[subsystem].clear();
		subsystemsIndeces[subsystem].clear();

		for (int j=partitionIndex; j<numPartitions_; j++) {

			if (j>partitionIndex)
				indexInPartition = -1;

			it = std::find(eventCountersStock[j].begin()+indexInPartition+1, eventCountersStock[j].end(), subsystem);
			if (it==eventCountersStock[j].end()) {
				if (j>partitionIndex) break;
			} else {
				indexInPartition = it - eventCountersStock[j].begin();
				subsystemsIndeces[subsystem].push_back((size_t)indexInPartition);
				partitionsDistribution[subsystem].push_back(j);
			}
		} // end of j loop

		if (partitionsDistribution[subsystem].empty()==false) {
			partitionIndex = partitionsDistribution[subsystem].back();

		} else {
			if (partitionIndex==0) {
				partitionsDistribution[subsystem].push_back(-1);
				partitionIndex = 0;
			} else {
				partitionsDistribution[subsystem].push_back(numPartitions_);
				partitionIndex = numPartitions_;
			}
		}

	} //  end of i loop

}

} // namespace ocs2