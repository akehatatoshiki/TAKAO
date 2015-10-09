package TAKAO;
import nativetest.*;

public class TAKAONative{
  static{
    System.loadLibrary("native");
  }
  public native int addPersistenceObject(Object pobj,String name);
  public native int deletePersistenceObject(String name);
  public native Object getPersistenceObject(String name);
  public native int isPersistence(String name);
  /* Method for cache */
  public native void clflush(Object ptr);

  /* Method for testing */
  public native void printObjectaddres(Object tgt);
  public native void compaddrHashtoRaw(Object tgt, String str);
  public native Class getClass(String name);
  public native void nilMethod();
  public native void turnOnTestingMode();

}
