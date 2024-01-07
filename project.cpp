#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <vector>
#include <set>
#include <regex>

#define FIRST_PART_ENABLE 1
#define SECOND_PART_ENABLE 0
#define CAN_RUN (FIRST_PART_ENABLE ^ SECOND_PART_ENABLE)
#define DEBUG 0

int main(int argc, char const *argv[])
{
    //Check correct amount of arguments
    if(argc != 4){
        std::cerr << "Not enough arguments! Retry!" << '\n';
        return 1;
    }

    //Open file handlers
    std::ifstream cacheFile(argv[1]);
    if(!cacheFile.is_open()){
        std::cerr << "Cannot open cache file!\n";
        return 1; 
    }
    std::ifstream refFile(argv[2]);
    if(!refFile.is_open()){
        std::cerr << "Cannot open reference file!\n";
        return 1;
    }
    std::ofstream outFile(argv[3]);
    if(!outFile.is_open()){
        std::cerr << "Cannot create output file!\n";
        return 1;
    }

    // cacheFile Variables
    int addressBits;
    int blockSize;
    int cacheSets;
    int associativity;

    // Read from cacheFile and close
    std::string cacheLine;
    int cacheData, cacheNum = 0;
    while (cacheFile >> cacheLine >> cacheData){
        if (cacheNum == 0)
            addressBits = cacheData;
        else if (cacheNum == 1)
            blockSize = cacheData;
        else if (cacheNum == 2)
            cacheSets = cacheData;
        else
            associativity = cacheData;
        cacheNum++;
    }
    cacheFile.close();

    // Calculate offset and indexBits count
    int offsetCount = log2(blockSize);
    int indexCount = log2(cacheSets);
    int blockCount = addressBits - offsetCount;

    #if(DEBUG)
    std::cout << "offsetCount: " << offsetCount << '\n';
    std::cout << "indexCount: " << indexCount << '\n';
    std::cout << "blockCount: " << blockCount << '\n';

    #endif

    // RefFile Variables
    std::vector<std::string> refList; //Reference List - Original
    std::vector<std::string> testcaseList; //Reference List - Modified

    // Read from refFile and close
    std::string refLine, header, tail;
    while (std::getline(refFile, refLine)){
        if (std::regex_search(refLine, std::regex(".benchmark"))){
            header = refLine;
            continue;
        }
        if (std::regex_search(refLine, std::regex(".end"))){
            tail = refLine;
            break;
        }
        refList.push_back(refLine);
    }
    refFile.close();

    //Retrieve Block_address
    for (int i = 0; i < refList.size(); i++){
        std::string str = refList[i].substr(0, blockCount);
        std::reverse(str.begin(), str.end()); //reverse to ease indexing.
        testcaseList.push_back(str);
    }

    #if(DEBUG)
        for(int i = 0;i < refList.size(); i++){
            std::cout << refList[i] << " " << testcaseList[i] << '\n';
        }
    #endif

    // First Part
    // use LSB indexing
    #if(FIRST_PART_ENABLE)
    std::vector <bool> indexBits(blockCount, false);
    for(int i = 0;i < indexCount; i++){
        indexBits[i] = true;
    }
    #endif

    // Second Part
    // use Zero-Cost indexing
    #if(SECOND_PART_ENABLE)
    std::pair<int, int> Q [blockCount];
    std::pair<int, int> C [blockCount][blockCount];

    for(int i = 0; i < blockCount; i++){
        int nil = 0, one = 0;

        // Count the zeros and ones
        for(int j = 0; j < testcaseList.size(); j++){
            for(int k = 0; k < blockCount; k++){
                if(testcaseList[j][k] == '0') 
                    nil += 1;
                else 
                    one += 1;
            }
        }
        // Determine the quality of each indexing bits.
        Q[i].first = std::min(nil, one);
        Q[i].second = std::max(nil, one);
    }

    //find equal bits between indexes.
    for(int i = 0; i < blockCount; i++){
        for(int j = i+1; j < blockCount; j++){
            int E = 0, D = 0;
            for(int k = 0; k < testcaseList.size(); k++){
                if(testcaseList[k][i] == testcaseList[k][j])
                    E++;
                else
                    D++;
            }

            C[i][j].first = std::min(E, D);
            C[i][j].second = std::max(E, D);

            //symmetry.
            C[j][i].first = std::min(E, D);
            C[j][i].second = std::max(E, D);
        }
    }

    bool indexBits[blockCount];
    bool S[blockCount];

    //initialize value
    for(int i = 0;i < blockCount; i++){
        indexBits[i] = false;
        S[i] = true;
    }
    
    //iterate for how many index bits needed.
    for(int i = 0; i < indexCount; i++){
        int best = 0; 
        double currBest = -1;

        //find best quality
        for(int j = 0; j < blockCount; j++){
            if(S[j] && (double)Q[j].first / (double)Q[j].second > currBest){
                currBest = (double)Q[j].first / (double)Q[j].second;
                best = j;
            }
        }
        S[best] = false; //mark as selected

        //produce new values
        for(int k = 0; k < blockCount; k++){
            Q[k].first *= C[best][k].first;
            Q[k].second *= C[best][k].second;
        }
        //select index
        indexBits[best] = true;
    }
    #endif

    #if(CAN_RUN)
        //CACHE SIMULATION - 1bit NRU Replacement Policy
        int cache[cacheSets][associativity];
        bool NRU[cacheSets][associativity];
        bool refHitList[refList.size()];
        int missCount = 0;

        //initialize values
        for(int i = 0;i < cacheSets; i++){
            for(int j = 0; j < associativity; j++){
                cache[i][j] = (1 << blockCount) + 1; // This should be fine?
                NRU[i][j] = true;
            }
        }
        for(int i = 0;i < refList.size(); i++){
            refHitList[i] = false;
        }

        //Iterate over all testcases
        for(int i = 0;i < testcaseList.size(); i++){
            //Find the index and tag bits.
            std::string indexStr, tagStr;
            for(int j = 0; j < blockCount; j++){
                if(indexBits[j])
                    indexStr += testcaseList[i][j];
                else
                    tagStr += testcaseList[i][j];
            }

            #if(DEBUG)
                std::cout << indexStr << " " << tagStr << '\n';
            #endif
            bool hitFlag = false;

            //convert to binary
            int index = std::stoi(indexStr, 0, 2);
            int tag = std::stoi(tagStr, 0, 2);

            //check for hit (sequentially)
            for(int a = 0; a < associativity; a++){
                if(cache[index][a] == tag){
                    hitFlag = true;
                    break;
                }
            }
            
            //mark answer
            refHitList[i] = hitFlag;
            if(hitFlag) continue;
            missCount++;
            
            //when missed, check for replace
            for(int a = 0; a < associativity; a++){
                if(NRU[index][a] == true){
                    NRU[index][a] = false;
                    cache[index][a] = tag;
                    break;
                }
                //if all entry in NRU is false, then reset all entries
                else if(a == associativity - 1 && NRU[index][a] == false){
                    for(int k = 0; k < associativity; k++){
                        NRU[index][k] = true;
                    }
                    //start again from 0
                    cache[index][0] = tag;
                    NRU[index][0] = false;
                }
            }
        }
    #endif

    //OUTPUT
    //Write cache information
    outFile << "Address bits: " << addressBits << '\n';
    outFile << "Block size: " << blockSize << '\n';
    outFile << "Cache sets: " << cacheSets << '\n';
    outFile << "Associativity: " << associativity << '\n' << '\n';

    //Write offset bits, indexing bits count and position.
    outFile << "Offset bit count: " << offsetCount << '\n';
    outFile << "Indexing bit count: " << indexCount << '\n';
    outFile << "Indexing bits:";
    for(int i = 0, k = 1; i < blockCount; i++){
        if(indexBits[i])
            outFile << " " << offsetCount + i;
    }
    outFile << '\n' << '\n';
    
    //Write reference list
    outFile << header << '\n';
    for(int i = 0;i < refList.size(); i++){
        outFile << refList[i] << ' ' << (refHitList[i]? "hit" : "miss") << '\n';
    }
    outFile << tail << '\n';
    outFile << '\n';

    //Write miss count
    outFile << "Total cache miss count: " << missCount << '\n';

    //Close file handler
    outFile.close();
    return 0;
}

/*
Author: Owen Elizmartin 許陞豪 110062163
Last Edit: 1/7/2024 11:23am
Github: https://github.com/s1mply-opel
*/