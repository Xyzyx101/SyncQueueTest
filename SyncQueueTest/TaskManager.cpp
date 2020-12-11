#include "TaskManager.h"

void TaskManager::Tick() {
	while(Running) {
		std::vector<Message> newMessages;
		if(In.TryGetAll(newMessages)) {
			Messages.insert(Messages.end(), newMessages.begin(), newMessages.end());
		}

	}
}
