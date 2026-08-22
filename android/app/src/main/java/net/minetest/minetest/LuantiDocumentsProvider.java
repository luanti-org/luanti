package net.minetest.minetest;

import static android.provider.DocumentsContract.Document;
import static android.provider.DocumentsContract.Root;

import android.database.Cursor;
import android.database.MatrixCursor;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsProvider;
import android.util.Log;

import androidx.annotation.NonNull;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Locale;
import java.util.regex.Pattern;

public class LuantiDocumentsProvider extends DocumentsProvider {
	private String dataDirectory;

	private static final String[] DEFAULT_ROOT_PROJECTION = new String[] {
		Root.COLUMN_ROOT_ID, Root.COLUMN_MIME_TYPES, Root.COLUMN_FLAGS, Root.COLUMN_ICON, Root.COLUMN_TITLE, Root.COLUMN_DOCUMENT_ID
	};

	private static final String[] DEFAULT_DOCUMENT_PROJECTION = new String[] {
		Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME, Document.COLUMN_MIME_TYPE, Document.COLUMN_LAST_MODIFIED, Document.COLUMN_FLAGS, Document.COLUMN_SIZE
	};

	private static final Pattern TEXT_FILE_REGEX = Pattern.compile("\\.(lua|txt|md|example|conf|po|tr|json)$");

	@Override
	public boolean onCreate()
	{
		try {
			dataDirectory = Utils.getUserDataDirectory(getContext()).getCanonicalPath();
		} catch (IOException e) {
			Log.w("LuantiDocumentsProvider", e);
			return false;
		}
		return new File(dataDirectory).isDirectory();
	}

	@Override
	public Cursor queryRoots(String[] projection)
	{
		final MatrixCursor result = new MatrixCursor(resolveRootProjection(projection));
		final MatrixCursor.RowBuilder row = result.newRow();

		row.add(Root.COLUMN_ROOT_ID, "1"); // no particular meaning, we only have one root
		row.add(Root.COLUMN_FLAGS, Root.FLAG_LOCAL_ONLY);
		row.add(Root.COLUMN_ICON, R.mipmap.ic_launcher);
		row.add(Root.COLUMN_TITLE, getContext().getString(R.string.label));
		row.add(Root.COLUMN_DOCUMENT_ID, ".");

		return result;
	}

	/* the provider exposes relative paths, so we have these two helpers */
	private String makeRelative(@NonNull File file) throws IOException
	{
		final String base = dataDirectory;
		String fileAbs = file.getCanonicalPath();
		if (fileAbs.equals(base))
			return ".";
		if (fileAbs.length() <= base.length() || !fileAbs.startsWith(base + "/"))
			throw new IllegalArgumentException("path is not relative to base");
		return fileAbs.substring(base.length() + 1);
	}

	private File fromRelative(@NonNull String s)
	{
		final String base = dataDirectory;
		if (s.contains("../"))
			throw new IllegalArgumentException("no parent folder allowed");
		return new File(base + "/" + s);
	}

	private String guessMimeType(@NonNull String filename)
	{
		filename = filename.toLowerCase(Locale.ROOT);
		// Perform some basic guessing. This might help other apps handle the files correctly.
		// See Client::loadMedia() for media file types.
		if (TEXT_FILE_REGEX.matcher(filename).find() || filename.equals("world.mt"))
			return "text/plain";
		if (filename.endsWith(".png"))
			return "image/png";
		if (filename.endsWith(".jpg") || filename.endsWith(".jpeg"))
			return "image/jpeg";
		if (filename.endsWith(".ogg"))
			return "audio/ogg";
		return "application/octet-stream";
	}

	private void makeDocumentRow(@NonNull File file, @NonNull MatrixCursor.RowBuilder row)
	{
		final String relative;
		try {
			relative = makeRelative(file);
		} catch (IOException e) {
			Log.w("LuantiDocumentsProvider", "can't make relative: " + file.getAbsolutePath(), e);
			return; // document providers are not allowed to throw an IOException, so just bail out
		}
		row.add(Document.COLUMN_DOCUMENT_ID, relative);
		row.add(Document.COLUMN_DISPLAY_NAME, file.getName());
		if (file.isDirectory()) {
			row.add(Document.COLUMN_MIME_TYPE, Document.MIME_TYPE_DIR);
		} else {
			row.add(Document.COLUMN_MIME_TYPE, guessMimeType(file.getName()));
		}
		row.add(Document.COLUMN_LAST_MODIFIED, file.lastModified());
		row.add(Document.COLUMN_FLAGS, 0);
		row.add(Document.COLUMN_SIZE, file.length());
	}

	@Override
	public Cursor queryChildDocuments(String parentDocumentId, String[] projection, String sortOrder)
	{
		final MatrixCursor result = new MatrixCursor(resolveDocumentProjection(projection));
		final File parent = fromRelative(parentDocumentId);
		final File[] files = parent.listFiles();
		if (files == null)
			return result;
		for (File child : files) {
			if (child.getName().equals(".nomedia"))
				continue;
			makeDocumentRow(child, result.newRow());
		}
		return result;
	}

	@Override
	public Cursor queryDocument(String documentId, String[] projection) throws FileNotFoundException
	{
		final MatrixCursor result = new MatrixCursor(resolveDocumentProjection(projection));
		final File file = fromRelative(documentId);
		if (!file.exists())
			throw new FileNotFoundException("File not found");
		makeDocumentRow(file, result.newRow());
		return result;
	}

	@Override
	public ParcelFileDescriptor openDocument(final String documentId, final String mode, CancellationSignal signal)
		throws UnsupportedOperationException, FileNotFoundException
	{
		if (!mode.equals("r"))
			throw new UnsupportedOperationException("Only reading supported");
		final File file = fromRelative(documentId);
		if (!file.exists())
			throw new FileNotFoundException("File not found");
		return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
	}

	// Helper methods
	private static String[] resolveRootProjection(String[] projection)
	{
		return projection == null ? DEFAULT_ROOT_PROJECTION : projection;
	}

	private static String[] resolveDocumentProjection(String[] projection)
	{
		return projection == null ? DEFAULT_DOCUMENT_PROJECTION : projection;
	}
}
