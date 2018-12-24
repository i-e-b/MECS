namespace MecsCore.Runtime
{
    public static class Mega {
        public static long Bytes(int num) {
            return num * 1048576L;
        }
    }
    
    public static class Giga {
        public static long Bytes(int num) {
            return num * 1073741824L;
        }
    }
    public static class Kilo {
        public static long Bytes(int num) {
            return num * 1024L;
        }
    }
}