#include <iostream>
#include <fstream>
#include <windows.h>
#include <cstdio>
#include <vector>
#include <set>

unsigned threadAmount = 0;
HANDLE* Threads;

unsigned numbersAmount = 0;
std::vector<int> Numbers;

/* Вспомогательный массив */
std::vector<int> SupportArray;
HANDLE supportMutex;

unsigned numbersPerThread = 0;
unsigned remainNumbers = 0;

HANDLE mainThreadEvent = nullptr;
HANDLE* threadsEvents = nullptr;

/* ------------------------------------------------------------------------------------------------------------------ */

/* Перестановка элементов  */
std::vector<int>::iterator partition(std::vector<int>::iterator start, std::vector<int>::iterator end){
    /*auto median = start;
    std::advance(median, std::distance(start, end) / 2);

    auto tempLeft = start;
    auto tempRight = end;

    while(tempLeft <= tempRight){
        while(*tempLeft < *median)
            tempLeft++;
        while(*tempRight > *median)
            tempRight--;
        if(tempLeft >= tempRight)
            break;
        std::iter_swap(tempLeft++, tempRight--);
//        tempLeft++;
//        tempRight--;
    }
    return tempRight;*/
    auto pivot = *end;
    auto i = start;
    for(auto j = start; j < end; j++)
        if(*j <= pivot)
            std::iter_swap(i++, j);

    std::iter_swap(i, end);
    return i;
}
void quickSort(std::vector<int>::iterator start, std::vector<int>::iterator end){
    if(start < end){
        auto median = partition(start, end);
        quickSort(start, median - 1);
        quickSort(median + 1, end);
    }
}

/* Информация, которую передаем потокам */
class sortingData {
public:
    sortingData(const unsigned number, const unsigned size, unsigned border) : threadNumber(number), subarraySize(size), firstElem(border){
        this->mutex = CreateMutexA(nullptr, false, nullptr);
    };
    unsigned giveFirstElem() const{
        return this->firstElem;
    }
    unsigned giveSize() const{
        return this->subarraySize;
    }
    unsigned giveNumber() const{
        return this->threadNumber;
    }

    HANDLE mutex;

private:
    unsigned threadNumber;
    unsigned subarraySize;
    unsigned firstElem;
};
std::vector<sortingData*> subarrayInfos;

DWORD WINAPI thread_entry(void* param) {
    auto subarrayInfo = (sortingData*) param;
    auto threadNumber = subarrayInfo->giveNumber();

    auto subarrayStart = Numbers.begin();
    auto subarrayEnd = Numbers.begin();
    auto subarraySize = subarrayInfo->giveSize();
    std::advance(subarrayStart, subarrayInfo->giveFirstElem());
    std::advance(subarrayEnd,subarraySize + subarrayInfo->giveFirstElem() - 1);

    quickSort(subarrayStart, subarrayEnd);

    const unsigned division = threadAmount * threadAmount;
    /* Заполняем вспомогательный массив элементами-разделителями */
    for(unsigned i = 0; (i * numbersAmount) / division < subarraySize; i++) {
        WaitForSingleObject(supportMutex, INFINITE);

        SupportArray.push_back(*(subarrayStart + (i * numbersAmount) / division));

        ReleaseMutex(supportMutex);
    }

    /* Поток отсортировал свой участок массива */
    SetEvent(threadsEvents[threadNumber]);

    /* Ждем пока основной поток выполнит необходимые действия */
    WaitForSingleObject(mainThreadEvent, INFINITE);
    return 0;
}

int threadsCreate(){
    mainThreadEvent = CreateEventA(nullptr, true, false, "MainThreadEvent");

    Threads = new HANDLE[threadAmount];

    sortingData* temp = nullptr;    /* Информация, которую передаем потокам */
    unsigned subarrayStart = 0;
    subarrayInfos.reserve(threadAmount);

    threadsEvents = new HANDLE[threadAmount];

    SupportArray.reserve(numbersAmount / threadAmount + 1);
    supportMutex = CreateMutexA(nullptr, false, nullptr);

    for(unsigned threadNumber = 0; threadNumber < threadAmount; threadNumber++){
        threadsEvents[threadNumber] = CreateEventA(nullptr, true, false, nullptr/*"EventForThread" + std::to_string(threadNumber)*/);

        const unsigned subarraySize = numbersPerThread + ((remainNumbers != 0 && threadNumber < remainNumbers) ? 1 : 0);
        temp = new sortingData(threadNumber, subarraySize, subarrayStart);
        subarrayInfos.push_back(temp);

        subarrayStart += subarraySize;

        Threads[threadNumber] = CreateThread(nullptr, 0, thread_entry, (void*) temp, 0 , nullptr);
        if(Threads[threadNumber] == nullptr){
            std::cout << "Failed thread creation" << std::endl;
            return -1;
        }
    }

    /* Ждем когда потоки отсортируют свои участки массива */
    WaitForMultipleObjects(threadAmount, threadsEvents, true, INFINITE);

    /* Сортируем вспомогательный массив */
    quickSort(SupportArray.begin(), SupportArray.end() - 1);

    /* Собираем множество разделителей */
    std::set<int> borders;
    for(unsigned i = 1; i * threadAmount + threadAmount / 2 - 1 < SupportArray.size(); i++)
        borders.insert(*(SupportArray.begin() + i * threadAmount + threadAmount / 2 - 1));

    /* Вспомогательный массив больше не нужен, разрушаем */
    SupportArray.clear();
    SupportArray.shrink_to_fit();

    SetEvent(mainThreadEvent);

    return 0;
}

int threadsDelete(){
    WaitForMultipleObjects(threadAmount, Threads, true, INFINITE);
    sortingData* temp = nullptr;

    for(unsigned i = 0; i < threadAmount; i++) {
        CloseHandle(Threads[i]);
        delete subarrayInfos[i];
    }
    delete [] Threads;

    return 0;
}
/* ------------------------------------------------------------------------------------------------------------------ */
int readArguments(const std::string &filename = "input.txt"){
    std::ifstream input(filename);
    input >> threadAmount >> numbersAmount;
    Numbers.reserve(numbersAmount);
    int temp = 0;
    for(unsigned i = 0; i < numbersAmount; i++) {
        input >> temp;
        Numbers.push_back(temp);
    }
    input.close();
    return 0;
}
void writeResults(const std::string &outputName = "output.txt", const std::string &timeName = "time.txt"){
    std::ofstream output(outputName);
    output << threadAmount << std::endl << numbersAmount << std::endl;
    for(unsigned i = 0; i < numbersAmount; i++)
        output << Numbers[i] << " ";
    output.close();
    std::ofstream time(timeName);
//    time << timeWorking;
    time.close();
}
/* ------------------------------------------------------------------------------------------------------------------ */

/* Разделяем исхдоный массив на N равных частей.
 * На каждом отрезке запускаем стандартную сортировку. */

int main() {
    readArguments();

    numbersPerThread = numbersAmount / threadAmount;
    remainNumbers = numbersAmount % threadAmount;

    threadsCreate();
    threadsDelete();
    return 0;
}
