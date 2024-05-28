#include <string>
namespace OpenVDBReader

{
    class OpenVDBReaderClass
    {
    public:
        OpenVDBReaderClass();
		~OpenVDBReaderClass();
		void ReadVDBFile();
		void OpenVDBFile(std::wstring filePath);
		void LoadVDBFile();
    private:
        std::wstring m_filePath;
    };
}