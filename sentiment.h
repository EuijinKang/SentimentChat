#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "strmap.h"

// global counting how many documents have been used to calculate sentiment
long doc_count;

// 0 is idf table, 1 is positive sentiment table, 2 is negative sentiment table
StrMap* RequiredStrMaps[3];

// this function will find number of documents token appears in. should not care about upper case
StrMap* FrequencyHash(char* training){
    // saving individual lines read
    char data[2048];
    char senti[9];
    // opens file
    FILE *f = fopen(training, "r");
    // make hash table to save results
    StrMap* completeFreqTable = sm_new(10000);
    StrMap* pos_table = sm_new(10000);
    RequiredStrMaps[1] = pos_table;
    StrMap* neg_table = sm_new(10000);
    RequiredStrMaps[2] = neg_table;

    //initialize doc count

    doc_count = 0;

    // read file until the end, save the line in data, max size is 2048, from file f 
    while (fgets(data, 2048, f) != NULL){
        doc_count++;

        // to check if the word was already accounted for in this specific line
        StrMap* RepeatTable = sm_new(100);

        // make all lowercase
        for(int i = 0; i < strlen(data); i++){
            if (isupper(data[i])){
                data[i] = tolower(data[i]);
            }
        }

        // tokenize strings
        char* nulcheck;
        char keyword[2048];
        // pos or neg?        
        fgets(senti, 9 , f);
// initial strtok
        if ((nulcheck = strtok(data, " ")) != NULL){
            strcpy(keyword, nulcheck);
            //if positive, store in positive table
            if (strcmp(senti, "positive") == 0) {
                int value;
                char* ptr;
                int sizehash = sm_get(pos_table, keyword, NULL, 0);
                if (sizehash == 0){
                    sm_put(pos_table, keyword, "1");
                } else{
                char strvalue[8];
                sm_get(pos_table, keyword, strvalue, sizehash);
                value = strtol(strvalue, &ptr, 10);
                value++;
                sprintf(strvalue, "%d", value);
                sm_put(pos_table, keyword, strvalue);
                }
            } else {
                // else put in negative table
                int value;
                char* ptr2;
                int sizehash = sm_get(neg_table, keyword, NULL, 0);
                if (sizehash == 0){
                    sm_put(neg_table, keyword, "1");
                } else{
                char strvalue[10];
                sm_get(neg_table, keyword, strvalue, sizehash);
                value = strtol(strvalue, &ptr2, 10);
                value++;
                sprintf(strvalue, "%d", value);
                sm_put(neg_table, keyword, strvalue);
                }
            }
            
           // if not already found in this line, update hash table, and update repeat table so we don't add again
            if (sm_exists (RepeatTable, keyword) == 0){
                int value;
                char* ptr3;
                int sizehash = sm_get(completeFreqTable, keyword, NULL, 0);
                if (sizehash == 0){
                    sm_put(completeFreqTable, keyword, "1");
                    sm_put(RepeatTable, keyword, "1");
                } else{
                char strvalue[10];
                sm_get(completeFreqTable, keyword, strvalue, sizehash);
                value = strtol(strvalue, &ptr3, 10);
                value++;
                sprintf(strvalue, "%d", value);
                sm_put(completeFreqTable, keyword, strvalue);
                sm_put(RepeatTable, keyword, "1");
                }
                
            }
        }

        // while you're reading through stirng in line:
        while ((nulcheck = strtok(NULL, " ")) != NULL){

            strcpy(keyword, nulcheck);
            
            if (strcmp(senti, "positive") == 0) {
                int value;
                char* ptr;
                int sizehash = sm_get(pos_table, keyword, NULL, 0);
                if (sizehash == 0){
                    sizehash = 2048;
                    sm_put(pos_table, keyword, "1");
                } else{
                char strvalue[10];
                sm_get(pos_table, keyword, strvalue, sizehash);
                value = strtol(strvalue, &ptr, 10);
                value++;
                sprintf(strvalue, "%d", value);
                sm_put(pos_table, keyword, strvalue);
                }
            } else {
                int value;
                char* ptr2;
                int sizehash = sm_get(neg_table, keyword, NULL, 0);
                if (sizehash == 0){
                    sizehash = 2048;
                    sm_put(neg_table, keyword, "1");
                } else{
                char strvalue[10];
                sm_get(neg_table, keyword, strvalue, sizehash);
                value = strtol(strvalue, &ptr2, 10);
                value++;
                sprintf(strvalue, "%d", value);
                sm_put(neg_table, keyword, strvalue);
                }
            }
            
           // if not already found in this line, update hash table, and update repeat table so we don't add again
            if (sm_exists (RepeatTable, keyword) == 0){
                int value;
                char* ptr3;
                int sizehash = sm_get(completeFreqTable, keyword, NULL, 0);
                if (sizehash == 0){
                    sizehash = 2048;
                    sm_put(completeFreqTable, keyword, "1");
                    sm_put(RepeatTable, keyword, "1");
                } else{
                char strvalue[10];
                sm_get(completeFreqTable, keyword, strvalue, sizehash);
                value = strtol(strvalue, &ptr3, 10);
                value++;
                sprintf(strvalue, "%d", value);
                sm_put(completeFreqTable, keyword, strvalue);
                sm_put(RepeatTable, keyword, "1");
                }
                
            }
        }


    }
    // close file once done
    fclose(f);
    // return the frequency table
    return completeFreqTable;
}

//enum function for idf conversion
void idfConvert(const char *key, const char *value, const void *obj){
    //walk through hash table and update each value
    char copy_val[100];
    char* ptr;
    double mutable_val;
    strcpy(copy_val, value);

    mutable_val = log10(doc_count / strtol(copy_val, &ptr, 10));
    sprintf(copy_val, "%f", mutable_val);
    sm_put(RequiredStrMaps[0], key, copy_val);
    
}

// takes in hash table and converts values into idf values
void idfConversion(StrMap* freqTable){
    StrMap* globalIDFtable = sm_new(10000);
    RequiredStrMaps[0] = globalIDFtable;
    sm_enum(freqTable, idfConvert, NULL);
}

double probabilities[2];

void TFIDFcalc(const char *key, const char *value, const void *obj){
    int IDFval;
    char* ptr;
    int sizehash = sm_get(RequiredStrMaps[0], key, NULL, 0);
    char IDFvalstr[10];
    if (sm_get(RequiredStrMaps[0], key, IDFvalstr, sizehash) == 0){
        IDFval = 0;
    }
    IDFval = strtol(IDFvalstr, &ptr, 10);

    char TFvalstr[10];
    double TFval;
    char* ptr2;
    strcpy(TFvalstr, value);

    TFval = strtol(TFvalstr, &ptr2, 10);

    long TFIDF = log10(TFval + 1) * log10(IDFval);

    double posTF;
    char* ptr3;
    int sizehash2 = sm_get(RequiredStrMaps[1], key, NULL, 0);
    char PosValStr[10];
    if (sm_get(RequiredStrMaps[1], key, PosValStr, sizehash2) == 0){
        posTF = 0;
    } else {
        posTF = strtol(PosValStr, &ptr3, 10);
    }

    long TFIDFpos = log10(posTF+1) * log10(IDFval);

    probabilities[0] = probabilities[0] + (double) (pow((TFIDFpos - TFIDF), 2));

    double negTF;
    char* ptr4;
    int sizehash3 = sm_get(RequiredStrMaps[2], key, NULL, 0);
    char NegValStr[10];
    if (sm_get(RequiredStrMaps[2], key, NegValStr, sizehash3) == 0){
        negTF = 0;
    } else{
        negTF = strtol(NegValStr, &ptr4, 10);
    }

    long TFIDFneg = log10(negTF+1) * log10(IDFval);

    probabilities[1] = probabilities[1] + (double) (pow((TFIDFneg - TFIDF), 2));
}

//give out sentiment val
double sentiment_val (char line[2048]){

    StrMap* TFtable = sm_new(1000);

    char* nulcheck;
    char token[2048];

    if ((nulcheck = strtok(line, " ")) != NULL){
        strcpy(token, nulcheck);
        int value;
        char* ptr;
        int sizehash = sm_get(TFtable, token, NULL, 0);
        if (sizehash == 0){
            sm_put(TFtable, token, "1");
        } else{
        char strvalue[10];
        sm_get(TFtable, token, strvalue, sizehash);
        value = strtol(strvalue, &ptr, 10);
        value++;
        sprintf(strvalue, "%d", value);
        sm_put(TFtable, token, strvalue);
        }
    }

        // while you're reading through stirng in line:
        while ((nulcheck = strtok(NULL, " "))!= NULL){
            char* ptr2;
            strcpy(token, nulcheck);

            int value;
            int sizehash = sm_get(TFtable, token, NULL, 0);
            if (sizehash == 0){
            sm_put(TFtable, token, "1");
        } else{
            char strvalue[10];
            sm_get(TFtable, token, strvalue, sizehash);
            value = strtol(strvalue, &ptr2, 10);
            value++;
            sprintf(strvalue, "%d", value);
            sm_put(TFtable, token, strvalue);
        }
        }    

    probabilities[0] = 0;
    probabilities[1] = 0;

    //tf-idf, then complete calc
    sm_enum (TFtable, TFIDFcalc, NULL);

    double pos_dist = sqrt(fabs(probabilities[0]));

    double neg_dist = sqrt(fabs(probabilities[1]));

    if (pos_dist > neg_dist){
        return pos_dist;
    } else{
        return -(neg_dist);
    }
}

void SetupFunct (){
    StrMap* freqTable = FrequencyHash("twitter-training-set-c");
    idfConversion(freqTable);
}

char* testingResults(char* test){
    // saving individual lines read
    char data[2048];

    int correct = 0;
    int total = 0;

    // opens file
    FILE *f = fopen(test, "r");

    SetupFunct();

    // read file until the end, save the line in data, max size is 3000, from file f 
    while (fgets(data, 2048, f) != NULL){
        total++;
        double sentiment = sentiment_val (data);
        fgets(data, 10, f);
        if ((sentiment > 0 && strcmp(data, "positive") == 0) || (sentiment < 0 && strcmp(data, "negative") == 0)){
            correct++;
        }
    }
    char* result = (char*) malloc(sizeof(char) * 500);
    sprintf (result, "The sentiment analyzer is at: %d / %d correct", correct, total);
    fclose(f);
    return result;
}