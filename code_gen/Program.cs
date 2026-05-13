// ========================================
// Code Generation Tool
// Developed with Claude Haiku 4.5
// ========================================

using code_gen;
using System;

namespace code_gen
{
    class Program
    {
        static void Main(string[] args)
        {
            // 인자: <모드> <입력경로> <출력경로>
            if (args.Length < 3)
            {
                PrintUsage();
                return;
            }

            string mode = args[0].ToLower();
            string inputPath = args[1];
            string outputPath = args[2];

            ICodegen? generator = null;

            // 모드에 따른 클래스 할당
            switch (mode)
            {
                case "packet":
                    generator = new ZppBitsPacketCodegen();
                    break;

                // 나중에 추가할 곳
                // case "table":
                //     generator = new TableCodegen();
                //     break;

                default:
                    Console.WriteLine($"Unknown mode: {mode}");
                    return;
            }

            try
            {
                generator.Generate(inputPath, outputPath);
                Console.WriteLine("Codegen task completed successfully.");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error during generation: {ex.Message}");
            }
        }

        static void PrintUsage()
        {
            Console.WriteLine("Usage: CodegenTool.exe <mode> <inputPath> <outputPath>");
            Console.WriteLine("Modes:");
            Console.WriteLine("  packet : Generate C++ packet structs from definitions");
        }
    }
}