using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace code_gen
{
    public interface ICodegen
    {
        void Generate(string inputPath, string outputPath);
    }
}
