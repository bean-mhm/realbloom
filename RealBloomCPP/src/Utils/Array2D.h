#pragma once

#include <vector>

#include "Misc.h"

template <typename T>
class Array2D
{
private:
    std::vector<T> m_vector;
    size_t m_rows = 0;
    size_t m_cols = 0;

public:
    Array2D() {};

    std::vector<T>& getVector();
    size_t getNumRows() const;
    size_t getNumCols() const;

    void reset();
    void resize(size_t numRows, size_t numCols);

    T& operator()(const size_t& row, const size_t& col);
};

template <typename T>
std::vector<T>& Array2D<T>::getVector()
{
    return m_vector;
}

template <typename T>
size_t Array2D<T>::getNumRows() const
{
    return m_rows;
}

template <typename T>
size_t Array2D<T>::getNumCols() const
{
    return m_cols;
}

template <typename T>
void Array2D<T>::reset()
{
    clearVector(m_vector);
}

template <typename T>
void Array2D<T>::resize(size_t numRows, size_t numCols)
{
    m_rows = numRows;
    m_cols = numCols;
    m_vector.resize(m_rows * m_cols);
}

template <typename T>
T& Array2D<T>::operator()(const size_t& row, const size_t& col)
{
    return m_vector[row * m_cols + col];
}
