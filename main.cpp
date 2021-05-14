#include <iostream>
#include <fstream>
#include <windows.h>
#include <vector>
#include <algorithm>

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

/* Информация, которую передаем потокам */
class sortingData {
public:
    sortingData(const unsigned number, const unsigned size, unsigned border) :
            threadNumber(number),
            subarraySize(size),
            firstElem(border){
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

private:
    unsigned threadNumber;
    unsigned subarraySize;
    unsigned firstElem;
};
std::vector<sortingData*> threadInfos;

std::vector<int> borders;

/* Время, потраченное на работу алгоритма */
DWORD timeBefore;
DWORD timeSpent;

template<class T>
void vectorDelete(std::vector<T>* array){
    array->clear();
    array->shrink_to_fit();
}
/* ------------------------------------------------------------------------------------------------------------------ */
template <typename RandomIt>
void QuickSort(RandomIt first, RandomIt last)
{
    if (std::distance(first, last) > 1){
            auto pivot = std::prev(last, 1);
            auto bound = first;
            for (auto j = first; j != pivot; ++j){
                if (*j < *pivot){
                    std::swap(*bound++, *j);
                }
            }
            std::swap(*bound, *pivot);
            QuickSort(first, bound);
            QuickSort(bound+1, last);
        }
}
/* ------------------------------------------------------------------------------------------------------------------ */
class SubarrayData {
public:

    void append(const int value){
        WaitForSingleObject(Mutex,INFINITE);
        array.push_back(value);
        ReleaseMutex(Mutex);
    }

    void appendMultiple(std::vector<int>::iterator start,
                        std::vector<int>::iterator end,
                        const int rightBorder,
                        const int leftBorder){
        WaitForSingleObject(Mutex, INFINITE);
        for(auto iter = start; iter != end; iter++)
            if(*iter <= rightBorder && *iter > leftBorder)
                array.push_back(*iter);
        ReleaseMutex(Mutex);
    }

    void appendMultipleLess(std::vector<int>::iterator start,
                            std::vector<int>::iterator end,
                            const int rightBorder){
        WaitForSingleObject(Mutex, INFINITE);
        for(auto iter = start; iter != end; iter++)
            if(*iter <= rightBorder)
                array.push_back(*iter);
        ReleaseMutex(Mutex);
    }

    void appendMultipleGreater(std::vector<int>::iterator start,
                               std::vector<int>::iterator end,
                               const int leftBorder){
        WaitForSingleObject(Mutex, INFINITE);
        for(auto iter = start; iter != end; iter++)
            if(*iter > leftBorder)
                array.push_back(*iter);
        ReleaseMutex(Mutex);
    }

    void sort(){
        std::sort(array.begin(), array.end());
    }


    void writeResults(std::ofstream &output){
        for(int i = 0; i < array.size(); i++)
            output << array[i] << " ";
    }

    SubarrayData(){
        array.reserve(numbersPerThread / 2 * 3);
        Mutex = CreateMutexA(nullptr, false, nullptr);
    }
    ~SubarrayData(){
        CloseHandle(Mutex);
        array.clear();
        array.shrink_to_fit();
    }

private:
    std::vector<int> array;
    HANDLE Mutex;
};
std::vector<SubarrayData*> Subarrays;
/* ------------------------------------------------------------------------------------------------------------------ */
int readArguments(const std::string &filename = "input.txt"){
    std::ifstream input(filename);
    if(input.fail()){
        std::cout << "Errors with input file." << std::endl;
        return -1;
    }
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
void writeWithThreads(const std::string &outputName = "output.txt", const std::string &timeName = "time.txt"){
    std::ofstream output(outputName);
    output << threadAmount << std::endl << numbersAmount << std::endl;
    for(auto i = 0; i < Subarrays.size(); i++){
        Subarrays[i]->writeResults(output);
    }
    output.close();

    std::ofstream time(timeName);
    time << timeSpent;
    time.close();
}
void writeSimple(const std::string &outputName = "output.txt", const std::string &timeName = "time.txt"){
    std::ofstream output(outputName);
    output << threadAmount << std::endl << numbersAmount << std::endl;
    for(auto i = 0; i < Numbers.size(); i++)
        output << Numbers[i] << " ";
    output.close();

    std::ofstream time(timeName);
    time << timeSpent;
    time.close();
}
/* ------------------------------------------------------------------------------------------------------------------ */

void ResetMultipleEvents(HANDLE* events, const unsigned size){
    for(auto i = 0; i < size; i++)
        ResetEvent(events[i]);
}
void prepareData(){
    /* Событие для главного потока. */
    mainThreadEvent = CreateEventA(nullptr, true, false, "MainThreadEvent");

    /* События для вспомогательных потоков. */
    threadsEvents = new HANDLE[threadAmount];
    /* Массив с данными для потоков. */
    threadInfos.reserve(threadAmount);

    unsigned subarrayStart = 0;
    for(unsigned threadNumber = 0; threadNumber < threadAmount; threadNumber++){
        threadsEvents[threadNumber] = CreateEventA(nullptr, true, false, nullptr);

        const unsigned subarraySize = numbersPerThread + ((remainNumbers != 0 && threadNumber < remainNumbers) ? 1 : 0);
        auto temp = new sortingData(threadNumber, subarraySize, subarrayStart);
        threadInfos.push_back(temp);

        subarrayStart += subarraySize;
    }

    /* Вспомогательный массив для главного потока. */
    SupportArray.reserve(numbersAmount / threadAmount + 1);
    supportMutex = CreateMutexA(nullptr, false, nullptr);

    Subarrays.reserve(threadAmount);
    for(unsigned i = 0; i < threadAmount; i++){
        auto* temp = new SubarrayData();
        Subarrays.push_back(temp);
    }
    borders.reserve(threadAmount + 1);
}

void deleteData(){
    vectorDelete(&SupportArray);
    vectorDelete(&borders);
    for(auto i = 0; i < threadAmount; i++) {
        delete Subarrays[i];
        Subarrays[i] = nullptr;
        delete threadInfos[i];
        threadInfos[i] = nullptr;
        CloseHandle(threadsEvents[i]);
    }
    vectorDelete(&Subarrays);
    CloseHandle(supportMutex);
    vectorDelete(&SupportArray);
    vectorDelete(&threadInfos);
    delete threadsEvents;
    CloseHandle(mainThreadEvent);
}

DWORD WINAPI thread_entry(void* param) {
    /* ------------------------------------------------------------ */
    auto subarrayInfo = (sortingData*) param;
    auto threadNumber = subarrayInfo->giveNumber();
    auto subarraySize = subarrayInfo->giveSize();

    auto subarrayStart = Numbers.begin();
    auto subarrayEnd = Numbers.begin();

    std::advance(subarrayStart, subarrayInfo->giveFirstElem());
    std::advance(subarrayEnd,subarraySize + subarrayInfo->giveFirstElem());

    if(subarrayEnd > subarrayStart)
//        QuickSort(subarrayStart, subarrayEnd);
        std::sort(subarrayStart, subarrayEnd);

    const unsigned division = threadAmount * threadAmount;
    /* Заполняем вспомогательный массив элементами-разделителями */
    for(unsigned i = 0; (i * numbersAmount) / division < subarraySize; i++) {
        const unsigned index = (i * numbersAmount) / division;
        WaitForSingleObject(supportMutex, INFINITE);

        SupportArray.push_back(*(subarrayStart + index));

        ReleaseMutex(supportMutex);
    }

    /* Поток сделал необходимые действия */
    SetEvent(threadsEvents[threadNumber]);
    /* Ждем пока основной поток подаст сигнал к действию */
    WaitForSingleObject(mainThreadEvent, INFINITE);

    /* Формируем вспомогательные подмассивы из подмассивов потоков
     * отностительно полученных разделителей  33 и 69:
     * Поток 1: 6 14 15 | 39 46 48 | 72 91 93
     * Поток 2: 12 21 | 36 40 54 61 69 | 89 97
     * Поток 3: 20 27 32 33 | 53 58 | 72 84 97
     *
     * Поток 1 - Меньше/равно 33: 6 14 15 12 21 20 27 32 33
     * Поток 2 - Больше 33, меньше/равно 69: 39 46 48 	36 40 54 61 69 	53 58
     * Поток 3 - Больше 69: 72 91 93 89 97 72 84 97     */

    /* Элементы, меньше первого разделителя,
     * элементы больше последнего разделителя */

    Subarrays[0]->appendMultipleLess(subarrayStart, subarrayEnd, borders[0]);
    Subarrays[borders.size() - 1]->appendMultipleGreater(subarrayStart, subarrayEnd, *borders.rbegin());

    /* Элементы, между несколькими разделителями */
    for(unsigned i = 1; i < borders.size(); i++)
        Subarrays[i]->appendMultiple(subarrayStart, subarrayEnd, borders[i], borders[i - 1]);

    /* Поток сделал свою работу */
    SetEvent(threadsEvents[threadNumber]);
    /* Ждем пока основной поток подаст сигнал к действию */
    WaitForSingleObject(mainThreadEvent, INFINITE);

    /* Сортируем полученные подмассивы элементов */
    Subarrays[threadNumber]->sort();

    /* Работа потока окончена */
    SetEvent(threadsEvents[threadNumber]);
    return 0;
}

int threadsCreate(){

    Threads = new HANDLE[threadAmount];

    for(unsigned threadNumber = 0; threadNumber < threadAmount; threadNumber++){
        Threads[threadNumber] = CreateThread(nullptr, 0, thread_entry,
                                             (void*) threadInfos[threadNumber], 0 , nullptr);
        if(Threads[threadNumber] == nullptr){
            std::cout << "Failed thread creation." << std::endl;
            return -1;
        }
    }

    /* Ждем когда потоки отсортируют свои участки массива */
    WaitForMultipleObjects(threadAmount, threadsEvents, true, INFINITE);
    ResetMultipleEvents(threadsEvents, threadAmount);

    /* Сортируем вспомогательный массив */
//    QuickSort(SupportArray.begin(), SupportArray.end());
    std::sort(std::begin(SupportArray), std::end(SupportArray));

    /* Собираем множество разделителей */
    for(unsigned i = 1; i * threadAmount + threadAmount / 2 - 1 < SupportArray.size(); i++)
        borders.push_back(*(SupportArray.begin() + i * threadAmount + threadAmount / 2 - 1));

    PulseEvent(mainThreadEvent);

    /* Ждем пока все потоки сделают необходимые действия */
    WaitForMultipleObjects(threadAmount, threadsEvents, true, INFINITE);
    ResetMultipleEvents(threadsEvents, threadAmount);

    /* Каждый поток распределил свои элементы между подмассивами */
    PulseEvent(mainThreadEvent);

    WaitForMultipleObjects(threadAmount, threadsEvents, true, INFINITE);
    return 0;
}

/* Реализован алгоритм, описанный в этой статье:
 * https://neerc.ifmo.ru/wiki/index.php?title=PSRS-сортировка. */

void stdOutTime(){
    std::cout << "Time - " << timeSpent << "ms, " << (double) timeSpent / 1000 << " seconds" << std::endl;
}

int main(const int argc, const char** argv) {
    if(argc == 2) {
        std::string filename(argv[1]);
        std::cout << "File - " << filename.c_str() << std::endl;
        if(readArguments(filename) == -1)
            return -1;
    } else
        if(readArguments() == -1)
            return -1;

    std::cout << "Threads number = " << threadAmount << std::endl;
    std::cout << "Numbers = " << numbersAmount << std::endl;

    numbersPerThread = numbersAmount / threadAmount;
    remainNumbers = numbersAmount % threadAmount;

    /* Если число дополнительных потоков - 1, нет смысла создавать второй поток.
     * Эффективнее отсортировать данные в main потоке.
     *
     * Если число потоков >= размеру массива, также нет смысла создавать потоки,
     * потому что каждый тогда будет сортировать один элемент массива. */

    if(threadAmount == 1 || threadAmount >= numbersAmount){
        timeBefore = GetTickCount();
        QuickSort(std::begin(Numbers), std::end(Numbers));
        timeSpent = GetTickCount() - timeBefore;
        stdOutTime();
        writeSimple();
    } else {
        prepareData();
        /* ----------------------------------- */
        timeBefore = GetTickCount();

        threadsCreate();

        timeSpent = GetTickCount() - timeBefore;
        /* ----------------------------------- */
        stdOutTime();
        writeWithThreads();
        deleteData();
    }
    return 0;
}
