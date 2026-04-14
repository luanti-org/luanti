import android.database.Cursor;
import android.database.MatrixCursor;
import android.provider.DocumentsProvider;

import java.io.FileNotFoundException;

public class LuantiDocumentsProvider extends DocumentsProvider {

  @Override
  public Cursor queryRoots(String[] projection) throws FileNotFoundException {
    
  }

  // helper method
  private MatrixCursor resolveRootProjection(String[] projection) {
    if (projection == null) 
  }
	
}
