#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/stat.h>
#include "iaea_phsp.h"    // functions operating on PHSP files
#include "iaea_header.h"  // header handling
#include "iaea_record.h"  // record (particle) operations
#include "utilities.h"    // helper functions

using namespace std;

// Filter parameters – units: cm
const float Z_PLANE = 100.0f; // z-axis level, e.g. 1000 mm
const float X_MIN = -7.0f;
const float X_MAX = 7.0f;
const float Y_MIN = -7.0f;
const float Y_MAX = 7.0f;

// Helper function: removes output files if they exist
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
    
    // Remove any existing output files for a clean start
    removeOutputFiles(outFile);
    
    IAEA_I32 src, dest, res;
    int lenIn = strlen(inFile);
    int lenOut = strlen(outFile);
    
    // Open input source in read-only mode (access = 1)
    IAEA_I32 accessRead = 1;
    iaea_new_source(&src, const_cast<char*>(inFile), &accessRead, &res, lenIn);
    if (res < 0) {
        cerr << "Error opening input source: " << inFile << endl;
        return 1;
    }
    
    // Check file size and byte order of input file.
    iaea_check_file_size_byte_order(&src, &res);
    if (res != 0) {
        // If error code is -3 (size mismatch but byte order ok), we issue a warning and proceed.
        if (res == -3) {
            cerr << "Warning: Input file size does not match header checksum (code " 
                 << res << "). Proceeding anyway." << endl;
            res = 0;
        } else {
            cerr << "Error: input file size or byte order mismatch (code " << res << ")." << endl;
            iaea_destroy_source(&src, &res);
            return 1;
        }
    }
    
    // Open output source in write mode (access = 2)
    IAEA_I32 accessWrite = 2;
    iaea_new_source(&dest, const_cast<char*>(outFile), &accessWrite, &res, lenOut);
    if (res < 0) {
        cerr << "Error creating output source: " << outFile << endl;
        iaea_destroy_source(&src, &res);
        return 1;
    }
    
    // Copy header from input file to output file
    iaea_copy_header(&src, &dest, &res);
    if (res < 0) {
        cerr << "Error copying header from input source." << endl;
        iaea_destroy_source(&src, &res);
        iaea_destroy_source(&dest, &res);
        return 1;
    }
    
    // Modify output header: disable extra data storage
    int zero = 0;
    iaea_set_extra_numbers(&dest, &zero, &zero);
    
    // Reset statistics – we will count only accepted records
    IAEA_I64 acceptedHistories = 0;
    IAEA_I64 acceptedParticles = 0;
    
    // Variables to hold particle record data.
    IAEA_I32 n_stat, partType;
    IAEA_Float E, wt, x, y, z, u, v, w;
    // In this example we ignore extra floats/longs.
    float dummyExtraFloats[NUM_EXTRA_FLOAT];
    IAEA_I32 dummyExtraInts[NUM_EXTRA_LONG];
    
    // Get expected number of records from header.
    IAEA_I64 expected;
    res = -1;
    iaea_get_max_particles(&src, &res, &expected);
    // Assume header contains one extra record – process expected - 1 records.
    IAEA_I64 expectedRecords = (expected > 0) ? expected - 1 : expected;
    cout << "Expected records (from header): " << expectedRecords << endl;
    
    cout << "Processing input file (" << inFile << ")..." << endl;
    
    // Loop: read records and apply filter
    IAEA_I64 count = 0;
    int errorCount = 0;
    const int ERROR_THRESHOLD = 10;
    
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
        // Filter condition:
        // If the particle is moving in the positive z direction and,
        // at z = Z_PLANE, its (x,y) falls within [X_MIN, X_MAX] x [Y_MIN, Y_MAX],
        // then accept (write) the particle.
        if (w > 0) {
            float newX = x;
            float newY = y;
            if (z < Z_PLANE) {
                float t = (Z_PLANE - z) / w;
                newX = x + u * t;
                newY = y + v * t;
            }
            if (newX >= X_MIN && newX <= X_MAX && newY >= Y_MIN && newY <= Y_MAX) {
                // Write accepted particle to output.
                iaea_write_particle(&dest, &n_stat, &partType, &E, &wt,
                                    &x, &y, &z, &u, &v, &w,
                                    dummyExtraFloats, dummyExtraInts);
                acceptedHistories++;
                acceptedParticles++;
            }
        }
        count++;
        if (count % 1000000 == 0)
            cout << "Processed " << count << " records." << endl;
    }
    
    cout << "Total records processed: " << count << endl;
    cout << "Accepted records (filtered): " << acceptedParticles << endl;
    
    // Update output header statistics based on accepted records.
    iaea_set_total_original_particles(&dest, &acceptedHistories);
    iaea_update_header(&dest, &res);
    if (res < 0)
        cerr << "Error updating output header (code " << res << ")." << endl;
    else
        cout << "Output header updated successfully." << endl;
    
    // Report output PHSP file size.
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
    
    // Clean up: close input and output sources.
    iaea_destroy_source(&src, &res);
    iaea_destroy_source(&dest, &res);
    
    cout << "Filtering complete." << endl;
    return 0;
}
