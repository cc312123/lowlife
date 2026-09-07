using System;
using System.IO;
using System.Runtime.InteropServices;

public class DiagTest {
    [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, ulong nSize, out IntPtr lpNumberOfBytesRead);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
    [DllImport("psapi.dll")] public static extern bool EnumProcessModulesEx(IntPtr hProcess, [Out] IntPtr[] lphModule, uint cb, out uint lpcbNeeded, uint dwFilterFlag);

    public static void Main() {
        string logPath = @"c:\Users\woah9\Downloads\my private-20260619T132210Z-3-001\my private\diag.txt";
        using (StreamWriter sw = new StreamWriter(logPath, false)) {
            try {
                System.Diagnostics.Process[] procs = System.Diagnostics.Process.GetProcessesByName("RobloxPlayerBeta");
                sw.WriteLine("Roblox procs found: " + procs.Length);
                if (procs.Length == 0) return;
                uint pid = (uint)procs[0].Id;
                sw.WriteLine("PID: " + pid);

                IntPtr hProc = OpenProcess(0x10 | 0x20 | 0x8 | 0x400, false, pid);
                sw.WriteLine("OpenProcess handle: " + hProc);
                if (hProc == IntPtr.Zero) return;

                IntPtr[] mods = new IntPtr[1024];
                uint cb;
                EnumProcessModulesEx(hProc, mods, (uint)(mods.Length * IntPtr.Size), out cb, 0x03);
                long baseAddr = mods[0].ToInt64();
                sw.WriteLine("BaseAddr: 0x" + baseAddr.ToString("X"));

                long fakeDmPtrAddr = baseAddr + 0x8d22868L;
                byte[] buf = new byte[8];
                IntPtr read;
                bool ok = ReadProcessMemory(hProc, new IntPtr(fakeDmPtrAddr), buf, 8, out read);
                long fakeDmVal = BitConverter.ToInt64(buf, 0);
                sw.WriteLine("FakeDataModel ptr (0x8d22868): 0x" + fakeDmPtrAddr.ToString("X") + " ok=" + ok + " val=0x" + fakeDmVal.ToString("X"));

                if (ok && fakeDmVal != 0) {
                    long realDmPtr = fakeDmVal + 0x1f8L;
                    ok = ReadProcessMemory(hProc, new IntPtr(realDmPtr), buf, 8, out read);
                    long realDmVal = BitConverter.ToInt64(buf, 0);
                    sw.WriteLine("RealDataModel (0x1f8): 0x" + realDmPtr.ToString("X") + " ok=" + ok + " val=0x" + realDmVal.ToString("X"));

                    if (ok && realDmVal != 0) {
                        long wsPtr = realDmVal + 0x158L;
                        ok = ReadProcessMemory(hProc, new IntPtr(wsPtr), buf, 8, out read);
                        long wsVal = BitConverter.ToInt64(buf, 0);
                        sw.WriteLine("Workspace (0x158): 0x" + wsPtr.ToString("X") + " ok=" + ok + " val=0x" + wsVal.ToString("X"));
                    }
                }

                long vePtrAddr = baseAddr + 0x8351408L;
                ok = ReadProcessMemory(hProc, new IntPtr(vePtrAddr), buf, 8, out read);
                long veVal = BitConverter.ToInt64(buf, 0);
                sw.WriteLine("VisualEngine ptr (0x8351408): 0x" + vePtrAddr.ToString("X") + " ok=" + ok + " val=0x" + veVal.ToString("X"));

                if (ok && veVal != 0) {
                    byte[] dimBuf = new byte[8];
                    ok = ReadProcessMemory(hProc, new IntPtr(veVal + 0xb10L), dimBuf, 8, out read);
                    float w = BitConverter.ToSingle(dimBuf, 0);
                    float h = BitConverter.ToSingle(dimBuf, 4);
                    sw.WriteLine("Dimensions (0xb10): ok=" + ok + " w=" + w + " h=" + h);

                    byte[] matBuf = new byte[64];
                    ok = ReadProcessMemory(hProc, new IntPtr(veVal + 0x1b0L), matBuf, 64, out read);
                    float m00 = BitConverter.ToSingle(matBuf, 0);
                    float m11 = BitConverter.ToSingle(matBuf, 20);
                    sw.WriteLine("ViewMatrix (0x1b0): ok=" + ok + " m00=" + m00 + " m11=" + m11);
                }

                CloseHandle(hProc);
            } catch (Exception ex) {
                sw.WriteLine("Exception: " + ex.ToString());
            }
        }
    }
}