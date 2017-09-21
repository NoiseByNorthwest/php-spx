#ifndef SPX_UTILS_H_DEFINED
#define SPX_UTILS_H_DEFINED

#define SPX_UTILS_TOKENIZE_STRING(str, delim, token, size, block) \
do {                                                              \
    const char * c_ = str;                                        \
    const char delim_ = delim;                                    \
    size_t i_ = 0;                                                \
    char token[size] = {0};                                       \
    while (1) {                                                   \
        if (*c_ == 0 || *c_ == delim_) {                          \
            token[i_] = 0;                                        \
                                                                  \
            block                                                 \
                                                                  \
            if (*c_ == 0) {                                       \
                break;                                            \
            }                                                     \
                                                                  \
            i_ = 0;                                               \
        } else if (i_ < sizeof(token) - 1) {                      \
            token[i_] = *c_;                                      \
            i_++;                                                 \
        }                                                         \
                                                                  \
        c_++;                                                     \
    }                                                             \
} while (0)

#endif /* SPX_UTILS_H_DEFINED */
