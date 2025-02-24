#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/stat.h>
#include "iaea_phsp.h"    // functions operating on PHSP files
#include "iaea_header.h"  // header handling
#include "iaea_record.h"  // operations on records (particles)
#include "utilities.h"    // utility functions

using namespace std;

// Filter parameters – units: cm
const float Z_PLANE = 100.0f; // z-axis level, e.g. 1000 mm
const float X_MIN = -7.0f;
const float X_MAX = 7.0f;
const float Y_MIN = -7.0f;
const float Y_MAX = 7.0f;

// Helper function: removes output files if they already exist
void removeOutputFiles(const char* baseName) {
    string headerFile = string(baseName) + ".IAEAheader";
    string phspFile   = string(baseName) + ".IAEAphsp";
    remove(headerFile.c_str());
    remove(phspFile.c_str());
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <inputFileBase> <outputFileBase>" << endl;
        return 1;
    }
    
    // First argument – input file base name (without extension)
    // Second argument – output file base name (without extension)
    const char* inFile = argv[1];
    const char* outFile = argv[2];
    
    // Remove any existing output files to ensure a clean start
    removeOutputFiles(outFile);
    
    IAEA_I32 src, dest;
    IAEA_I32 res;
    int lenIn = strlen(inFile);
    int lenOut = strlen(outFile);
    
    // Open the input file in read mode (access = 1)
    IAEA_I32 accessRead = 1;
    iaea_new_source(&src, const_cast<char*>(inFile), &accessRead, &res, lenIn);
    if (res < 0) {
        cerr << "Error opening input source: " << inFile << endl;
        return 1;
    }
    
    // Check file size and byte order compatibility
    iaea_check_file_size_byte_order(&src, &res);
    if (res != 0) {
        cerr << "Error: input file size or byte order mismatch (code " << res << ")." << endl;
        iaea_destroy_source(&src, &res);
        return 1;
    }
    
    // Open the output file in write mode (access = 2)
    IAEA_I32 accessWrite = 2;
    iaea_new_source(&dest, const_cast<char*>(outFile), &accessWrite, &res, lenOut);
    if (res < 0) {
        cerr << "Error creating output source: " << outFile << endl;
        iaea_destroy_source(&src, &res);
        return 1;
    }
    
    // Copy the header from the input file to the output file
    iaea_copy_header(&src, &dest, &res);
    if (res < 0) {
        cerr << "Error copying header from input source." << endl;
        iaea_destroy_source(&src, &res);
        iaea_destroy_source(&dest, &res);
        return 1;
    }
    
    // Modify the output header: disable storage of extra data
    int zero = 0;
    iaea_set_extra_numbers(&dest, &zero, &zero);
    
    // Also update statistics – reset the counts for original histories and particles
    // We start with 0 and will sum up the accepted records.
    IAEA_I64 acceptedHistories = 0;
    IAEA_I64 acceptedParticles = 0;
    
    // Read records – we will filter them based on a condition
    IAEA_I32 n_stat, partType;
    IAEA_Float E, wt, x, y, z, u, v, w;
    // In this example, extra floats/longs are ignored (not written)
    float dummyExtraFloats[NUM_EXTRA_FLOAT];
    IAEA_I32 dummyExtraInts[NUM_EXTRA_LONG];
    
    // Counters for records read and errors encountered
    IAEA_I64 count = 0;
    int errorCount = 0;
    
    cout << "Processing input file (" << inFile << ")..." << endl;
    
    // Retrieve the expected number of records from the header
    IAEA_I64 expected;
    res = -1;
    iaea_get_max_particles(&src, &res, &expected);
    // Assume the header contains one extra record – read expected - 1 records
    IAEA_I64 expectedRecords = (expected > 0) ? expected - 1 : expected;
    cout << "Expected records (from header): " << expectedRecords << endl;
    
    // Particle reading loop
    for (IAEA_I64 i = 0; i < expectedRecords; i++) {
        iaea_get_particle(&src, &n_stat, &partType, &E, &wt,
                          &x, &y, &z, &u, &v, &w,
                          dummyExtraFloats, dummyExtraInts);
        if (n_stat == -1) {
            errorCount++;
            cerr << "Error reading particle at record " << i 
                 << " (error count: " << errorCount << ")" << endl;
            if (errorCount > ERROR_THRESHOLD) {
                cerr << "Too many errors. Aborting filtering." << endl;
                break;
            }
            continue;
        }
        // Filter: recalculate (x,y) position at the Z_PLANE level
        float newX, newY;
        if (w > 0) {
            if (z < Z_PLANE) {
                float t = (Z_PLANE - z) / w;
                newX = x + u * t;
                newY = y + v * t;
            } else {
                newX = x;
                newY = y;
            }
            // Check if (newX, newY) falls within the defined rectangle
            if (newX >= X_MIN && newX <= X_MAX && newY >= Y_MIN && newY <= Y_MAX) {
                // Condition met – write the particle record
                iaea_write_particle(&dest, &n_stat, &partType, &E, &wt,
                                    &x, &y, &z, &u, &v, &w,
                                    dummyExtraFloats, dummyExtraInts);
                acceptedHistories++;
                acceptedParticles++;
            }
        }
        // If w <= 0 – the particle is not moving in the positive z-direction; skip it.
        count++;
        if (count % 1000000 == 0)
            cout << "Processed " << count << " records." << endl;
    }
    
    cout << "Total records processed: " << count << endl;
    cout << "Accepted records (filtered): " << acceptedParticles << endl;
    
    // Update the output header:
    // Set the number of original histories based on the accepted records.
    iaea_set_total_original_particles(&dest, &acceptedHistories);
    // Update the particle count in the header (usually iaea_update_header uses internal counters)
    iaea_update_header(&dest, &res);
    if (res < 0)
        cerr << "Error updating output header (code " << res << ")." << endl;
    else
        cout << "Output header updated successfully." << endl;
    
    // Diagnostics: read the output PHSP file size based on its name
    string outPhspPath = string(outFile) + ".IAEAphsp";
    struct stat fileStatus;
    FILE* fp = fopen(outPhspPath.c_str(), "rb");
    if (fp) {
        if (fstat(fileno(fp), &fileStatus) == 0) {
            IAEA_I64 fileSize = fileStatus.st_size;
            cout << "Output PHSP file size: " << fileSize << " bytes." << endl;
        }
        fclose(fp);
    } else {
        cerr << "Cannot open output PHSP file for size check: " << outPhspPath << endl;
    }
    
    // Close input and output sources
    iaea_destroy_source(&src, &res);
    iaea_destroy_source(&dest, &res);
    
    cout << "Filtering complete." << endl;
    return 0;
}
